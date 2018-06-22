from __future__ import print_function
from Interface import Interface
from time import sleep
import sys
import serial
import threading
import time
import math
import RNS

class KISS():
	FEND			= chr(0xC0)
	FESC			= chr(0xDB)
	TFEND			= chr(0xDC)
	TFESC			= chr(0xDD)
	
	CMD_UNKNOWN		= chr(0xFE)
	CMD_DATA		= chr(0x00)
	CMD_FREQUENCY	= chr(0x01)
	CMD_BANDWIDTH	= chr(0x02)
	CMD_TXPOWER		= chr(0x03)
	CMD_SF			= chr(0x04)
	CMD_CR          = chr(0x05)
	CMD_RADIO_STATE = chr(0x06)
	CMD_RADIO_LOCK	= chr(0x07)
	CMD_DETECT		= chr(0x08)
	CMD_PROMISC		= chr(0x0E)
	CMD_READY       = chr(0x0F)
	CMD_STAT_RX		= chr(0x21)
	CMD_STAT_TX		= chr(0x22)
	CMD_STAT_RSSI	= chr(0x23)
	CMD_BLINK		= chr(0x30)
	CMD_RANDOM		= chr(0x40)
	CMD_FW_VERSION  = chr(0x50)
	CMD_ROM_READ    = chr(0x51)

	DETECT_REQ      = chr(0x73)
	DETECT_RESP     = chr(0x46)
	
	RADIO_STATE_OFF = chr(0x00)
	RADIO_STATE_ON	= chr(0x01)
	RADIO_STATE_ASK = chr(0xFF)
	
	CMD_ERROR		    = chr(0x90)
	ERROR_INITRADIO     = chr(0x01)
	ERROR_TXFAILED	    = chr(0x02)
	ERROR_EEPROM_LOCKED	= chr(0x03)

	@staticmethod
	def escape(data):
		data = data.replace(chr(0xdb), chr(0xdb)+chr(0xdd))
		data = data.replace(chr(0xc0), chr(0xdb)+chr(0xdc))
		return data
	

class RNodeInterface():
	MAX_CHUNK = 32768
	FREQ_MIN  = 137000000
	FREQ_MAX  = 1020000000

	LOG_CRITICAL = 0
	LOG_ERROR    = 1
	LOG_WARNING  = 2
	LOG_NOTICE   = 3
	LOG_INFO     = 4
	LOG_VERBOSE  = 5
	LOG_DEBUG    = 6
	LOG_EXTREME  = 7

	def __init__(self, callback, name, port, frequency = None, bandwidth = None, txpower = None, sf = None, cr = None, loglevel = -1, flow_control = True):
		self.serial      = None
		self.loglevel    = loglevel
		self.callback    = callback
		self.name        = name
		self.port        = port
		self.speed       = 115200
		self.databits    = 8
		self.parity      = serial.PARITY_NONE
		self.stopbits    = 1
		self.timeout     = 100
		self.online      = False

		self.frequency   = frequency
		self.bandwidth   = bandwidth
		self.txpower     = txpower
		self.sf          = sf
		self.cr          = cr
		self.state       = KISS.RADIO_STATE_OFF
		self.bitrate     = 0

		self.r_frequency = None
		self.r_bandwidth = None
		self.r_txpower   = None
		self.r_sf        = None
		self.r_cr        = None
		self.r_state     = None
		self.r_lock      = None
		self.r_stat_rx   = None
		self.r_stat_tx   = None
		self.r_stat_rssi = None
		self.r_random	 = None

		self.packet_queue    = []
		self.flow_control    = flow_control
		self.interface_ready = False

		self.validcfg  = True
		if (self.frequency < RNodeInterface.FREQ_MIN or self.frequency > RNodeInterface.FREQ_MAX):
			self.log("Invalid frequency configured for "+str(self), RNodeInterface.LOG_ERROR)
			self.validcfg = False

		if (self.txpower < 0 or self.txpower > 17):
			self.log("Invalid TX power configured for "+str(self), RNodeInterface.LOG_ERROR)
			self.validcfg = False

		if (self.bandwidth < 7800 or self.bandwidth > 500000):
			self.log("Invalid bandwidth configured for "+str(self), RNodeInterface.LOG_ERROR)
			self.validcfg = False

		if (self.sf < 7 or self.sf > 12):
			self.log("Invalid spreading factor configured for "+str(self), RNodeInterface.LOG_ERROR)
			self.validcfg = False

		if (not self.validcfg):
			raise ValueError("The configuration for "+str(self)+" contains errors, interface is offline")

		try:
			self.log("Opening serial port "+self.port+"...")
			self.serial = serial.Serial(
				port = self.port,
				baudrate = self.speed,
				bytesize = self.databits,
				parity = self.parity,
				stopbits = self.stopbits,
				xonxoff = False,
				rtscts = False,
				timeout = 0,
				inter_byte_timeout = None,
				write_timeout = None,
				dsrdtr = False,
			)
		except Exception as e:
			self.log("Could not open serial port for interface "+str(self), RNodeInterface.LOG_ERROR)
			raise e

		if self.serial.is_open:
			sleep(2.0)
			thread = threading.Thread(target=self.readLoop)
			thread.setDaemon(True)
			thread.start()
			self.online = True
			self.log("Serial port "+self.port+" is now open")
			self.log("Configuring RNode interface...", RNodeInterface.LOG_VERBOSE)
			self.initRadio()
			if (self.validateRadioState()):
				self.interface_ready = True
				self.log(str(self)+" is configured and powered up")
				sleep(1.0)
			else:
				self.log("After configuring "+str(self)+", the actual radio parameters did not match your configuration.", RNodeInterface.LOG_ERROR)
				self.log("Make sure that your hardware actually supports the parameters specified in the configuration", RNodeInterface.LOG_ERROR)
				self.log("Aborting RNode startup", RNodeInterface.LOG_ERROR)
				self.serial.close()
				raise IOError("RNode interface did not pass validation")
		else:
			raise IOError("Could not open serial port")

	def log(self, message, level):
		pass

	def initRadio(self):
		self.setFrequency()
		self.setBandwidth()
		self.setTXPower()
		self.setSpreadingFactor()
		self.setRadioState(KISS.RADIO_STATE_ON)

	def setFrequency(self):
		c1 = self.frequency >> 24
		c2 = self.frequency >> 16 & 0xFF
		c3 = self.frequency >> 8 & 0xFF
		c4 = self.frequency & 0xFF
		data = KISS.escape(chr(c1)+chr(c2)+chr(c3)+chr(c4))

		kiss_command = KISS.FEND+KISS.CMD_FREQUENCY+data+KISS.FEND
		written = self.serial.write(kiss_command)
		if written != len(kiss_command):
			raise IOError("An IO error occurred while configuring frequency for "+self(str))

	def setBandwidth(self):
		c1 = self.bandwidth >> 24
		c2 = self.bandwidth >> 16 & 0xFF
		c3 = self.bandwidth >> 8 & 0xFF
		c4 = self.bandwidth & 0xFF
		data = KISS.escape(chr(c1)+chr(c2)+chr(c3)+chr(c4))

		kiss_command = KISS.FEND+KISS.CMD_BANDWIDTH+data+KISS.FEND
		written = self.serial.write(kiss_command)
		if written != len(kiss_command):
			raise IOError("An IO error occurred while configuring bandwidth for "+self(str))

	def setTXPower(self):
		txp = chr(self.txpower)
		kiss_command = KISS.FEND+KISS.CMD_TXPOWER+txp+KISS.FEND
		written = self.serial.write(kiss_command)
		if written != len(kiss_command):
			raise IOError("An IO error occurred while configuring TX power for "+self(str))

	def setSpreadingFactor(self):
		sf = chr(self.sf)
		kiss_command = KISS.FEND+KISS.CMD_SF+sf+KISS.FEND
		written = self.serial.write(kiss_command)
		if written != len(kiss_command):
			raise IOError("An IO error occurred while configuring spreading factor for "+self(str))

	def setCodingRate(self):
		cr = chr(self.cr)
		kiss_command = KISS.FEND+KISS.CMD_CR+cr+KISS.FEND
		written = self.serial.write(kiss_command)
		if written != len(kiss_command):
			raise IOError("An IO error occurred while configuring coding rate for "+self(str))

	def setRadioState(self, state):
		kiss_command = KISS.FEND+KISS.CMD_RADIO_STATE+state+KISS.FEND
		written = self.serial.write(kiss_command)
		if written != len(kiss_command):
			raise IOError("An IO error occurred while configuring radio state for "+self(str))

	def validateRadioState(self):
		self.log("Validating radio configuration for "+str(self)+"...", RNodeInterface.LOG_VERBOSE)
		sleep(0.25);
		if (self.frequency != self.r_frequency):
			self.log("Frequency mismatch", RNodeInterface.LOG_ERROR)
			self.validcfg = False
		if (self.bandwidth != self.r_bandwidth):
			self.log("Bandwidth mismatch", RNodeInterface.LOG_ERROR)
			self.validcfg = False
		if (self.txpower != self.r_txpower):
			self.log("TX power mismatch", RNodeInterface.LOG_ERROR)
			self.validcfg = False
		if (self.sf != self.r_sf):
			self.log("Spreading factor mismatch", RNodeInterface.LOG_ERROR)
			self.validcfg = False

		if (self.validcfg):
			return True
		else:
			return False

	def setPromiscuousMode(self, state):
		if state == True
			kiss_command = KISS.FEND+KISS.CMD_PROMISC+chr(0x01)+KISS.FEND
		else:
			kiss_command = KISS.FEND+KISS.CMD_PROMISC+chr(0x00)+KISS.FEND

		written = self.serial.write(kiss_command)
		if written != len(kiss_command):
			raise IOError("An IO error occurred while configuring promiscuous mode for "+self(str))


	def updateBitrate(self):
		try:
			self.bitrate = self.r_sf * ( (4.0/self.cr) / (math.pow(2,self.r_sf)/(self.r_bandwidth/1000)) ) * 1000
			self.bitrate_kbps = round(self.bitrate/1000.0, 2)
			self.log(str(self)+" On-air bitrate is now "+str(self.bitrate_kbps)+ " kbps", RNodeInterface.LOG_DEBUG)
		except:
			self.bitrate = 0

	def processIncoming(self, data):
		self.callback(data, self)

	def send(self, data):
		processOutgoing(data)

	def processOutgoing(self,data):
		if self.online:
			if self.interface_ready:
				if self.flow_control:
					self.interface_ready = False

				data = KISS.escape(data)
				frame = chr(0xc0)+chr(0x00)+data+chr(0xc0)
				written = self.serial.write(frame)
				if written != len(frame):
					raise IOError("Serial interface only wrote "+str(written)+" bytes of "+str(len(data)))
			else:
				self.queue(data)

	def queue(self, data):
		self.packet_queue.append(data)

	def process_queue(self):
		if len(self.packet_queue) > 0:
			data = self.packet_queue.pop(0)
			self.interface_ready = True
			self.processOutgoing(data)
		elif len(self.packet_queue) == 0:
			self.interface_ready = True

	def readLoop(self):
		try:
			in_frame = False
			escape = False
			command = KISS.CMD_UNKNOWN
			data_buffer = ""
			command_buffer = ""
			last_read_ms = int(time.time()*1000)

			while self.serial.is_open:
				if self.serial.in_waiting:
					byte = self.serial.read(1)
					last_read_ms = int(time.time()*1000)

					if (in_frame and byte == KISS.FEND and command == KISS.CMD_DATA):
						in_frame = False
						self.processIncoming(data_buffer)
						data_buffer = ""
						command_buffer = ""
					elif (byte == KISS.FEND):
						in_frame = True
						command = KISS.CMD_UNKNOWN
						data_buffer = ""
						command_buffer = ""
					elif (in_frame and len(data_buffer) < RNS.Reticulum.MTU):
						if (len(data_buffer) == 0 and command == KISS.CMD_UNKNOWN):
							command = byte
						elif (command == KISS.CMD_DATA):
							if (byte == KISS.FESC):
								escape = True
							else:
								if (escape):
									if (byte == KISS.TFEND):
										byte = KISS.FEND
									if (byte == KISS.TFESC):
										byte = KISS.FESC
									escape = False
								data_buffer = data_buffer+byte
						elif (command == KISS.CMD_FREQUENCY):
							if (byte == KISS.FESC):
								escape = True
							else:
								if (escape):
									if (byte == KISS.TFEND):
										byte = KISS.FEND
									if (byte == KISS.TFESC):
										byte = KISS.FESC
									escape = False
								command_buffer = command_buffer+byte
								if (len(command_buffer) == 4):
									self.r_frequency = ord(command_buffer[0]) << 24 | ord(command_buffer[1]) << 16 | ord(command_buffer[2]) << 8 | ord(command_buffer[3])
									self.log(str(self)+" Radio reporting frequency is "+str(self.r_frequency/1000000.0)+" MHz", RNodeInterface.LOG_DEBUG)
									self.updateBitrate()

						elif (command == KISS.CMD_BANDWIDTH):
							if (byte == KISS.FESC):
								escape = True
							else:
								if (escape):
									if (byte == KISS.TFEND):
										byte = KISS.FEND
									if (byte == KISS.TFESC):
										byte = KISS.FESC
									escape = False
								command_buffer = command_buffer+byte
								if (len(command_buffer) == 4):
									self.r_bandwidth = ord(command_buffer[0]) << 24 | ord(command_buffer[1]) << 16 | ord(command_buffer[2]) << 8 | ord(command_buffer[3])
									self.log(str(self)+" Radio reporting bandwidth is "+str(self.r_bandwidth/1000.0)+" KHz", RNodeInterface.LOG_DEBUG)
									self.updateBitrate()

						elif (command == KISS.CMD_TXPOWER):
							self.r_txpower = ord(byte)
							self.log(str(self)+" Radio reporting TX power is "+str(self.r_txpower)+" dBm", RNodeInterface.LOG_DEBUG)
						elif (command == KISS.CMD_SF):
							self.r_sf = ord(byte)
							self.log(str(self)+" Radio reporting spreading factor is "+str(self.r_sf), RNodeInterface.LOG_DEBUG)
							self.updateBitrate()
						elif (command == KISS.CMD_CR):
							self.r_cr = ord(byte)
							self.log(str(self)+" Radio reporting coding rate is "+str(self.r_cr), RNodeInterface.LOG_DEBUG)
							self.updateBitrate()
						elif (command == KISS.CMD_RADIO_STATE):
							self.r_state = ord(byte)
						elif (command == KISS.CMD_RADIO_LOCK):
							self.r_lock = ord(byte)
						elif (command == KISS.CMD_STAT_RX):
							if (byte == KISS.FESC):
								escape = True
							else:
								if (escape):
									if (byte == KISS.TFEND):
										byte = KISS.FEND
									if (byte == KISS.TFESC):
										byte = KISS.FESC
									escape = False
								command_buffer = command_buffer+byte
								if (len(command_buffer) == 4):
									self.r_stat_rx = ord(command_buffer[0]) << 24 | ord(command_buffer[1]) << 16 | ord(command_buffer[2]) << 8 | ord(command_buffer[3])

						elif (command == KISS.CMD_STAT_TX):
							if (byte == KISS.FESC):
								escape = True
							else:
								if (escape):
									if (byte == KISS.TFEND):
										byte = KISS.FEND
									if (byte == KISS.TFESC):
										byte = KISS.FESC
									escape = False
								command_buffer = command_buffer+byte
								if (len(command_buffer) == 4):
									self.r_stat_tx = ord(command_buffer[0]) << 24 | ord(command_buffer[1]) << 16 | ord(command_buffer[2]) << 8 | ord(command_buffer[3])

						elif (command == KISS.CMD_STAT_RSSI):
							self.r_stat_rssi = ord(byte)
						elif (command == KISS.CMD_RANDOM):
							self.r_random = ord(byte)
						elif (command == KISS.CMD_ERROR):
							if (byte == KISS.ERROR_INITRADIO):
								self.log(str(self)+" hardware initialisation error (code "+RNS.hexrep(byte)+")", RNodeInterface.LOG_ERROR)
							elif (byte == KISS.ERROR_INITRADIO):
								self.log(str(self)+" hardware TX error (code "+RNS.hexrep(byte)+")", RNodeInterface.LOG_ERROR)
							else:
								self.log(str(self)+" hardware error (code "+RNS.hexrep(byte)+")", RNodeInterface.LOG_ERROR)
						elif (command == KISS.CMD_READY):
							# TODO: add timeout and reset if ready
							# command never arrives
							self.process_queue()
						
				else:
					time_since_last = int(time.time()*1000) - last_read_ms
					if len(data_buffer) > 0 and time_since_last > self.timeout:
						self.log(str(self)+" serial read timeout", RNodeInterface.LOG_DEBUG)
			 			data_buffer = ""
			 			in_frame = False
			 			command = KISS.CMD_UNKNOWN
			 			escape = False
			 		sleep(0.08)

		except Exception as e:
			self.online = False
			self.log("A serial port error occurred, the contained exception was: "+str(e), RNodeInterface.LOG_ERROR)
			self.log("The interface "+str(self.name)+" is now offline.", RNodeInterface.LOG_ERROR)

	def log(msg, level=3):
		if self.loglevel >= level:
			timestamp = time.time()
			logstring = "["+time.strftime(logtimefmt)+"] ["+self.loglevelname(level)+"] "+msg

			if (logdest == LOG_STDOUT):
				print(logstring)

	def loglevelname(level):
		if (level == RNodeInterface.LOG_CRITICAL):
			return "Critical"
		if (level == RNodeInterface.LOG_ERROR):
			return "Error"
		if (level == RNodeInterface.LOG_WARNING):
			return "Warning"
		if (level == RNodeInterface.LOG_NOTICE):
			return "Notice"
		if (level == RNodeInterface.LOG_INFO):
			return "Info"
		if (level == RNodeInterface.LOG_VERBOSE):
			return "Verbose"
		if (level == RNodeInterface.LOG_DEBUG):
			return "Debug"
		if (level == RNodeInterface.LOG_EXTREME):
			return "Extra"

	def __str__(self):
		return "RNodeInterface["+self.name+"]"

