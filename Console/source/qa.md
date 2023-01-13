[title]: <> (Questions & Answers)
## Questions & Answers
This section contains a list of common questions, and associated answers.

- **What are the system requirements for running Reticulum?**  
Practically any system that can run Python3 can also run Reticulum. Any computer made since the early 2000's should work, provided it has a reasonably up-to-date operating system installed. Even low-power embedded devices with 256 megabytes of RAM will run Reticulum.
- **Does Reticulum work without the Internet?**  
Yes. Reticulum *is* itself both a networking, and an inter-net protocol. A key difference between Reticulum and IPv4/v6, however, is that Reticulum does not require any central coordination or authority to work. As soon as two devices running Reticulum can talk to each other, they form a network. That network can dynamically grow to planetary-scale nets, split up, re-connect and heal in any number of ways, while still continuing to function. As long as there is *some sort of physical way* for two or more devices to communicate, Reticulum will allow them to form a secure and reliable network.
- **Who owns and controls the addresses I use on a Reticulum network?**  
You do. Every address is in complete ownership and control of the person that created it.
- **If nobody centrally controls the addresses, will my address still be globally reachable?**  
Yes. Reticulum ensures end-to-end connectivity. All addresses are globally and directly reachable. Reticulum has no concept of "private address spaces" and NAT, as you might be suffering from with IPv4.
- **Is communication over Reticulum encrypted?**  
Yes. All traffic is end-to-end encrypted. Reticulum *is fundamentally unable to route unencrypted traffic*. Links established over Reticulum networks offer forward secrecy, by using ephemeral encryption keys.  
- **Could you build a global Internet with Reticulum instead of IP?**  
Yes. In theory this is completely possible, but it will take a lot of refinement, development, hardware support and adoption to transition the global base-layer for communication to Reticulum. Please [help us]({ASSET_PATH}contribute.html) towards this goal!  
- **Is Reticulum as fast and optimised as my favorite TCP/IP stack?**  
Currently not, but we are working towards being much faster than IP. The primary focus of Reticulum has been to build an understandable and well-documented *reference implementation*, that works exceptionally well over medium-bandwidth to extremely low-bandwidth forms of communication. This focus is very valuable, since it allows people to build secure communications networks that span vast areas, with very simple hardware, and very little cost.
- **Who created all of this?**  
The Reticulum protocol, and the RNode system was created by [Mark Qvist]({ASSET_PATH}contact.html), of [unsigned.io](https://unsigned.io).