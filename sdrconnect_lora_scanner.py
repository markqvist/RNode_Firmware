#!/usr/bin/env python3
"""
SDRconnect Headless WebSocket client for LoRa signal detection at 868 MHz.

Connects to SDRconnect_headless via its WebSocket API (port 5454 by default),
tunes the RSPduo to 868 MHz, and monitors signal power + spectrum data to
detect LoRa activity.

Usage:
    # First, start SDRconnect_headless:
    #   /opt/sdrconnect/SDRconnect_headless --websocket_port=5454
    #
    # Then run this script:
    #   python3 sdrconnect_lora_scanner.py
    #   python3 sdrconnect_lora_scanner.py --host 127.0.0.1 --port 5454
    #   python3 sdrconnect_lora_scanner.py --save-spectrum spectrum.png
    #   python3 sdrconnect_lora_scanner.py --iq-capture iq_data.bin --iq-seconds 5

Protocol reference: SDRconnect WebSocket API 1.0.1
    https://www.sdrplay.com/docs/SDRconnect_WebSocket_API.pdf
"""

import argparse
import asyncio
import json
import struct
import sys
import time
from collections import deque
from datetime import datetime

import numpy as np

try:
    import websockets
except ImportError:
    print("ERROR: 'websockets' package required. Install with: pip3 install websockets")
    sys.exit(1)


# --- SDRconnect WebSocket API message helpers ---

def make_msg(event_type: str, property_name: str = "", value: str = "") -> str:
    """Build a JSON message for the SDRconnect WebSocket API."""
    return json.dumps({
        "event_type": event_type,
        "property": property_name,
        "value": value
    })


def set_property(prop: str, value) -> str:
    return make_msg("set_property", prop, str(value))


def get_property(prop: str) -> str:
    return make_msg("get_property", prop)


# --- Binary message parser ---

PAYLOAD_TYPE_AUDIO = 1      # Signed 16-bit PCM Stereo @ 48kHz (LRLR)
PAYLOAD_TYPE_IQ = 2         # Signed 16-bit interleaved IQ (IQIQ)
PAYLOAD_TYPE_SPECTRUM = 3   # Unsigned 8-bit spectrum FFT bins


def parse_binary_message(data: bytes):
    """
    Parse a binary WebSocket message from SDRconnect.
    Returns (payload_type, payload_data).

    Binary messages start with a 2-byte little-endian uint16 payload type:
        1 = Signed 16-bit PCM Stereo Audio @ 48kHz (LRLR)
        2 = Signed 16-bit interleaved IQ (IQIQ)
        3 = Unsigned 8-bit spectrum FFT bins normalised to visible range
    """
    if len(data) < 2:
        return None, None
    payload_type = struct.unpack_from('<H', data, 0)[0]
    payload = data[2:]
    return payload_type, payload


class SDRconnectClient:
    """Async client for SDRconnect WebSocket API."""

    def __init__(self, host: str = "127.0.0.1", port: int = 5454):
        self.uri = f"ws://{host}:{port}/"
        self.ws = None
        self.properties = {}
        self._property_events = {}  # property name -> asyncio.Event
        self._property_values = {}  # property name -> latest value

        # Data buffers
        self.spectrum_bins = deque(maxlen=200)  # last N spectrum snapshots
        self.iq_buffer = bytearray()
        self.signal_power_history = deque(maxlen=1000)
        self.signal_snr_history = deque(maxlen=1000)

        # Stats
        self.spectrum_count = 0
        self.iq_sample_count = 0

    async def connect(self):
        """Establish WebSocket connection."""
        print(f"Connecting to {self.uri} ...")
        self.ws = await websockets.connect(
            self.uri,
            max_size=10 * 1024 * 1024,  # 10 MB max message
            open_timeout=10,
        )
        print("Connected.")

    async def close(self):
        if self.ws:
            await self.ws.close()
            print("Connection closed.")

    async def send(self, msg: str):
        """Send a JSON text message."""
        await self.ws.send(msg)

    async def send_set_property(self, prop: str, value):
        await self.send(set_property(prop, value))

    async def send_get_property(self, prop: str):
        await self.send(get_property(prop))

    async def request_property(self, prop: str, timeout: float = 5.0) -> str:
        """Send get_property and wait for the response."""
        evt = asyncio.Event()
        self._property_events[prop] = evt
        await self.send_get_property(prop)
        try:
            await asyncio.wait_for(evt.wait(), timeout=timeout)
        except asyncio.TimeoutError:
            print(f"  WARNING: Timeout waiting for property '{prop}'")
            return None
        finally:
            self._property_events.pop(prop, None)
        return self._property_values.get(prop)

    def _handle_json(self, msg: dict):
        """Process an incoming JSON message."""
        event_type = msg.get("event_type", "")
        prop = msg.get("property", "")
        value = msg.get("value", "")

        if event_type == "property_changed":
            self.properties[prop] = value
            if prop == "signal_power":
                try:
                    self.signal_power_history.append(
                        (time.time(), float(value))
                    )
                except (ValueError, TypeError):
                    pass
            elif prop == "signal_snr":
                try:
                    self.signal_snr_history.append(
                        (time.time(), float(value))
                    )
                except (ValueError, TypeError):
                    pass
            # Wake up anyone waiting for this property
            if prop in self._property_events:
                self._property_values[prop] = value
                self._property_events[prop].set()

        elif event_type == "get_property_response":
            self._property_values[prop] = value
            if prop in self._property_events:
                self._property_events[prop].set()

    def _handle_binary(self, data: bytes):
        """Process an incoming binary message."""
        payload_type, payload = parse_binary_message(data)

        if payload_type == PAYLOAD_TYPE_SPECTRUM:
            bins = np.frombuffer(payload, dtype=np.uint8)
            self.spectrum_bins.append((time.time(), bins.copy()))
            self.spectrum_count += 1

        elif payload_type == PAYLOAD_TYPE_IQ:
            self.iq_buffer.extend(payload)
            # Count IQ sample pairs (each pair = 4 bytes: I16 + Q16)
            self.iq_sample_count += len(payload) // 4

    async def receive_loop(self, duration: float = None):
        """
        Receive messages in a loop. If duration is set, stop after that
        many seconds. Otherwise run until cancelled.
        """
        start = time.time()
        try:
            async for message in self.ws:
                if isinstance(message, str):
                    try:
                        msg = json.loads(message)
                        self._handle_json(msg)
                    except json.JSONDecodeError:
                        print(f"  WARNING: Non-JSON text message: {message[:100]}")
                elif isinstance(message, bytes):
                    self._handle_binary(message)

                if duration and (time.time() - start) >= duration:
                    break
        except websockets.ConnectionClosed as e:
            print(f"Connection closed: {e}")

    # --- High-level commands ---

    async def select_device(self, index: int = 0):
        """Select device by index."""
        print(f"Selecting device index {index} ...")
        await self.send(make_msg("selected_device", "", str(index)))
        await asyncio.sleep(1)

    async def start_device_streaming(self):
        """Start the device hardware streaming."""
        print("Starting device streaming ...")
        await self.send(make_msg("device_stream_enable", "", "true"))
        await asyncio.sleep(1)

    async def stop_device_streaming(self):
        """Stop the device hardware streaming."""
        await self.send(make_msg("device_stream_enable", "", "false"))

    async def enable_spectrum(self, enable: bool = True):
        val = "true" if enable else "false"
        print(f"Spectrum streaming: {val}")
        await self.send(make_msg("spectrum_enable", "", val))

    async def enable_iq_stream(self, enable: bool = True):
        val = "true" if enable else "false"
        print(f"IQ streaming: {val}")
        await self.send(make_msg("iq_stream_enable", "", val))

    async def tune(self, center_freq: int, vfo_freq: int = None,
                   sample_rate: float = 2e6, bandwidth: int = None):
        """
        Tune the radio.
        center_freq: LO frequency in Hz
        vfo_freq: VFO frequency in Hz (defaults to center_freq)
        sample_rate: sample rate in Hz
        bandwidth: filter bandwidth in Hz
        """
        if vfo_freq is None:
            vfo_freq = center_freq

        print(f"Setting center frequency: {center_freq/1e6:.3f} MHz")
        await self.send_set_property("device_center_frequency", center_freq)
        await asyncio.sleep(0.3)

        print(f"Setting VFO frequency: {vfo_freq/1e6:.3f} MHz")
        await self.send_set_property("device_vfo_frequency", vfo_freq)
        await asyncio.sleep(0.3)

        print(f"Setting sample rate: {sample_rate/1e6:.1f} MHz")
        await self.send_set_property("device_sample_rate", int(sample_rate))
        await asyncio.sleep(0.3)

        if bandwidth:
            print(f"Setting filter bandwidth: {bandwidth/1e3:.0f} kHz")
            await self.send_set_property("filter_bandwidth", bandwidth)
            await asyncio.sleep(0.3)

    async def configure_for_lora_868(self, sample_rate: float = 2e6):
        """
        Configure for LoRa monitoring at 868 MHz (EU ISM band).

        LoRa EU868 channels:
            868.1 MHz, 868.3 MHz, 868.5 MHz (common 3-channel)
            867.1, 867.3, 867.5, 867.7, 867.9 MHz (additional)
            869.525 MHz (downlink RX2)

        With 2 MHz sample rate centered at 868.3 MHz, we cover
        ~867.3 - 869.3 MHz which includes the 3 primary channels.
        """
        center = 868_300_000  # 868.3 MHz - center of primary LoRa channels
        await self.tune(
            center_freq=center,
            vfo_freq=center,
            sample_rate=sample_rate,
            bandwidth=int(sample_rate),
        )
        # Use NFM demodulator (closest to what we want for monitoring)
        print("Setting demodulator: NFM")
        await self.send_set_property("demodulator", "NFM")
        await asyncio.sleep(0.2)

    async def print_device_info(self):
        """Query and print current device state."""
        props = [
            "device_center_frequency", "device_vfo_frequency",
            "device_sample_rate", "filter_bandwidth",
            "lna_state", "lna_state_min", "lna_state_max",
            "demodulator", "started", "can_control",
            "signal_power", "signal_snr",
        ]
        print("\n--- Device State ---")
        for p in props:
            val = await self.request_property(p, timeout=3.0)
            if val is not None:
                self.properties[p] = val
                # Format frequency values nicely
                if "frequency" in p and val and val.isdigit():
                    freq_mhz = int(val) / 1e6
                    print(f"  {p}: {val} ({freq_mhz:.3f} MHz)")
                elif "sample_rate" in p and val:
                    try:
                        sr = float(val)
                        print(f"  {p}: {val} ({sr/1e6:.3f} MHz)")
                    except ValueError:
                        print(f"  {p}: {val}")
                else:
                    print(f"  {p}: {val}")
            else:
                print(f"  {p}: (no response)")
        print("---\n")


async def monitor_lora(args):
    """Main monitoring routine."""
    client = SDRconnectClient(host=args.host, port=args.port)

    try:
        await client.connect()

        # Select device (index 0 = first device)
        await client.select_device(args.device_index)

        # Start hardware streaming
        await client.start_device_streaming()
        await asyncio.sleep(1)

        # Tune to 868 MHz LoRa band
        await client.configure_for_lora_868(sample_rate=args.sample_rate)
        await asyncio.sleep(1)

        # Query current state
        # Start receive loop in background to handle property responses
        recv_task = asyncio.create_task(client.receive_loop())
        await asyncio.sleep(0.5)  # let loop start

        await client.print_device_info()

        # Enable spectrum streaming
        await client.enable_spectrum(True)

        # Optionally enable IQ streaming
        if args.iq_capture:
            await client.enable_iq_stream(True)

        # Cancel the generic receive loop, we'll run our own timed one
        recv_task.cancel()
        try:
            await recv_task
        except asyncio.CancelledError:
            pass

        # --- Main monitoring loop ---
        print(f"\nMonitoring 868 MHz LoRa band for {args.duration} seconds ...")
        print(f"  Center: 868.3 MHz | BW: {args.sample_rate/1e6:.1f} MHz")
        print(f"  Coverage: ~{(868.3 - args.sample_rate/2e6):.1f} - "
              f"{(868.3 + args.sample_rate/2e6):.1f} MHz")
        print()

        start_time = time.time()
        last_report = start_time
        report_interval = 2.0  # Print status every N seconds

        try:
            async for message in client.ws:
                now = time.time()
                elapsed = now - start_time

                if isinstance(message, str):
                    try:
                        msg = json.loads(message)
                        client._handle_json(msg)
                    except json.JSONDecodeError:
                        pass
                elif isinstance(message, bytes):
                    client._handle_binary(message)

                # Periodic status report
                if now - last_report >= report_interval:
                    last_report = now
                    # Get latest signal power/SNR from property_changed events
                    power = client.properties.get("signal_power", "?")
                    snr = client.properties.get("signal_snr", "?")
                    spec_count = client.spectrum_count
                    iq_count = client.iq_sample_count

                    ts = datetime.now().strftime("%H:%M:%S")
                    line = (f"[{ts}] t={elapsed:.0f}s | "
                            f"Power: {power} dB | SNR: {snr} dB | "
                            f"Spectrum frames: {spec_count}")
                    if args.iq_capture:
                        line += f" | IQ samples: {iq_count}"
                    print(line)

                    # Detect LoRa-like activity: sudden power increases
                    if client.signal_power_history:
                        recent = [p for t, p in client.signal_power_history
                                  if now - t < 5.0]
                        if len(recent) >= 2:
                            avg = sum(recent) / len(recent)
                            peak = max(recent)
                            if peak - avg > 6:  # 6 dB spike
                                print(f"  >>> SIGNAL DETECTED: "
                                      f"peak={peak:.1f} dB, avg={avg:.1f} dB, "
                                      f"delta={peak-avg:.1f} dB")

                if elapsed >= args.duration:
                    break

        except websockets.ConnectionClosed as e:
            print(f"Connection closed during monitoring: {e}")

        # --- Post-capture analysis ---
        print(f"\n--- Capture Summary ---")
        print(f"Duration: {args.duration} seconds")
        print(f"Spectrum frames captured: {client.spectrum_count}")
        print(f"IQ samples captured: {client.iq_sample_count}")

        if client.signal_power_history:
            powers = [p for _, p in client.signal_power_history]
            print(f"Signal power: min={min(powers):.1f} dB, "
                  f"max={max(powers):.1f} dB, "
                  f"mean={sum(powers)/len(powers):.1f} dB")

        if client.signal_snr_history:
            snrs = [s for _, s in client.signal_snr_history]
            print(f"Signal SNR: min={min(snrs):.1f} dB, "
                  f"max={max(snrs):.1f} dB, "
                  f"mean={sum(snrs)/len(snrs):.1f} dB")

        # Save spectrum plot if requested
        if args.save_spectrum and client.spectrum_bins:
            save_spectrum_plot(client, args)

        # Save IQ data if requested
        if args.iq_capture and client.iq_buffer:
            save_iq_data(client, args)

        # Analyze spectrum for LoRa signatures
        if client.spectrum_bins:
            analyze_spectrum_for_lora(client, args)

        # Cleanup
        await client.enable_spectrum(False)
        if args.iq_capture:
            await client.enable_iq_stream(False)
        await client.stop_device_streaming()

    finally:
        await client.close()


def save_spectrum_plot(client, args):
    """Save a waterfall/spectrum plot of captured data."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
    except ImportError:
        print("WARNING: matplotlib not available, skipping spectrum plot")
        return

    # Build waterfall from spectrum history
    timestamps = []
    spectra = []
    for t, bins in client.spectrum_bins:
        timestamps.append(t)
        spectra.append(bins)

    if not spectra:
        print("No spectrum data to plot.")
        return

    # Normalize to same length (take the most common length)
    lengths = [len(s) for s in spectra]
    target_len = max(set(lengths), key=lengths.count)
    spectra = [s for s in spectra if len(s) == target_len]
    if not spectra:
        return

    waterfall = np.array(spectra, dtype=np.float32)
    # FFT bins are uint8 normalised to visible range by SDRconnect
    # Higher values = stronger signal

    sr = args.sample_rate
    center = 868.3  # MHz
    freq_axis = np.linspace(center - sr/2e6, center + sr/2e6, target_len)
    time_axis = np.arange(len(waterfall))

    fig, axes = plt.subplots(2, 1, figsize=(12, 8), gridspec_kw={'height_ratios': [1, 2]})

    # Top: average spectrum
    avg_spectrum = np.mean(waterfall, axis=0)
    max_spectrum = np.max(waterfall, axis=0)
    axes[0].plot(freq_axis, avg_spectrum, 'b-', alpha=0.7, label='Mean')
    axes[0].plot(freq_axis, max_spectrum, 'r-', alpha=0.5, label='Peak')
    axes[0].set_xlabel('Frequency (MHz)')
    axes[0].set_ylabel('Magnitude (0-255)')
    axes[0].set_title(f'868 MHz LoRa Band Spectrum ({len(waterfall)} frames)')
    axes[0].legend()
    axes[0].grid(True, alpha=0.3)

    # Mark LoRa channel frequencies
    lora_channels = [868.1, 868.3, 868.5]
    for ch in lora_channels:
        if freq_axis[0] <= ch <= freq_axis[-1]:
            axes[0].axvline(x=ch, color='g', linestyle='--', alpha=0.5,
                          label=f'{ch} MHz' if ch == lora_channels[0] else '')
            axes[0].text(ch, axes[0].get_ylim()[1] * 0.95, f'{ch}',
                        ha='center', va='top', fontsize=8, color='green')

    # Bottom: waterfall
    extent = [freq_axis[0], freq_axis[-1], len(waterfall), 0]
    axes[1].imshow(waterfall, aspect='auto', extent=extent,
                   cmap='viridis', interpolation='nearest')
    axes[1].set_xlabel('Frequency (MHz)')
    axes[1].set_ylabel('Time (frame #)')
    axes[1].set_title('Waterfall')
    for ch in lora_channels:
        if freq_axis[0] <= ch <= freq_axis[-1]:
            axes[1].axvline(x=ch, color='r', linestyle='--', alpha=0.5)

    plt.tight_layout()
    outfile = args.save_spectrum
    plt.savefig(outfile, dpi=150)
    print(f"Spectrum plot saved to: {outfile}")
    plt.close()


def save_iq_data(client, args):
    """Save captured IQ data to a binary file."""
    outfile = args.iq_capture
    with open(outfile, 'wb') as f:
        f.write(bytes(client.iq_buffer))
    n_samples = len(client.iq_buffer) // 4  # 2 bytes I + 2 bytes Q
    print(f"IQ data saved to: {outfile} ({len(client.iq_buffer)} bytes, "
          f"{n_samples} IQ samples)")
    print(f"  Format: signed 16-bit little-endian interleaved IQ (IQIQ)")
    print(f"  To load in Python:")
    print(f"    raw = np.fromfile('{outfile}', dtype=np.int16)")
    print(f"    iq = raw[0::2] + 1j * raw[1::2]  # complex IQ")


def analyze_spectrum_for_lora(client, args):
    """Analyze captured spectrum data for LoRa-like signals."""
    if not client.spectrum_bins:
        return

    # Get spectra of consistent length
    lengths = [len(s) for _, s in client.spectrum_bins]
    target_len = max(set(lengths), key=lengths.count)
    spectra = [s for _, s in client.spectrum_bins if len(s) == target_len]
    if not spectra:
        return

    waterfall = np.array(spectra, dtype=np.float32)
    sr = args.sample_rate
    center = 868.3e6
    n_bins = target_len

    # Map bin indices to frequencies
    freqs = np.linspace(center - sr/2, center + sr/2, n_bins)

    # LoRa channel frequencies
    lora_channels = {
        "EU868 ch1": 868.1e6,
        "EU868 ch2": 868.3e6,
        "EU868 ch3": 868.5e6,
    }

    print(f"\n--- LoRa Channel Analysis ---")
    print(f"Spectrum bins: {n_bins}, frames: {len(spectra)}")

    # For each LoRa channel, check for activity
    # LoRa BW is typically 125 kHz or 250 kHz
    lora_bw = 125e3  # Hz, typical LoRa BW

    avg_spectrum = np.mean(waterfall, axis=0)
    noise_floor = np.median(avg_spectrum)
    print(f"Noise floor (median): {noise_floor:.1f}")

    for name, ch_freq in lora_channels.items():
        # Find bins covering this channel +/- half BW
        ch_mask = (freqs >= ch_freq - lora_bw) & (freqs <= ch_freq + lora_bw)
        if not np.any(ch_mask):
            print(f"  {name} ({ch_freq/1e6:.1f} MHz): outside capture range")
            continue

        ch_avg = np.mean(avg_spectrum[ch_mask])
        ch_max = np.max(avg_spectrum[ch_mask])
        ch_peak_frame = np.max(waterfall[:, ch_mask])

        # Check for intermittent signals (LoRa is bursty)
        ch_per_frame = np.mean(waterfall[:, ch_mask], axis=1)
        above_noise = ch_per_frame > noise_floor + 10  # 10 units above noise
        active_frames = np.sum(above_noise)
        duty_cycle = active_frames / len(spectra) * 100

        delta = ch_avg - noise_floor
        print(f"  {name} ({ch_freq/1e6:.1f} MHz):")
        print(f"    Avg level: {ch_avg:.1f} (delta from noise: {delta:+.1f})")
        print(f"    Peak level: {ch_max:.1f} (peak any frame: {ch_peak_frame:.1f})")
        print(f"    Active frames: {active_frames}/{len(spectra)} "
              f"({duty_cycle:.1f}% duty cycle)")

        if delta > 15:
            print(f"    ** STRONG signal activity detected **")
        elif delta > 5:
            print(f"    * Possible signal activity *")
        else:
            print(f"    (no significant activity)")

    print()


def main():
    parser = argparse.ArgumentParser(
        description="Monitor 868 MHz LoRa band via SDRconnect WebSocket API",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Prerequisites:
  1. Start SDRconnect_headless:
     /opt/sdrconnect/SDRconnect_headless --websocket_port=5454

  2. Run this script:
     python3 %(prog)s --duration 30 --save-spectrum lora_868.png

Examples:
  # Basic 30-second scan
  %(prog)s --duration 30

  # Save spectrum plot and IQ data
  %(prog)s --duration 60 --save-spectrum spectrum.png --iq-capture iq.bin

  # Different host/port
  %(prog)s --host 192.168.1.100 --port 5454 --duration 30
        """
    )
    parser.add_argument("--host", default="127.0.0.1",
                        help="SDRconnect WebSocket host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=5454,
                        help="SDRconnect WebSocket port (default: 5454)")
    parser.add_argument("--duration", type=float, default=30,
                        help="Monitoring duration in seconds (default: 30)")
    parser.add_argument("--sample-rate", type=float, default=2e6,
                        help="Sample rate in Hz (default: 2000000)")
    parser.add_argument("--device-index", type=int, default=0,
                        help="Device index to select (default: 0)")
    parser.add_argument("--save-spectrum", metavar="FILE",
                        help="Save spectrum/waterfall plot to PNG file")
    parser.add_argument("--iq-capture", metavar="FILE",
                        help="Capture raw IQ data to binary file")

    args = parser.parse_args()
    asyncio.run(monitor_lora(args))


if __name__ == "__main__":
    main()
