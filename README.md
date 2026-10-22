# Chevy-Bolt-EV-electric-car-12-Volt-Battery-Monitor
This program runs on custom hardware that attaches directly to the 12 volt battery of the Chevy Bolt EV

Version 1.2.0 detects when a single reading is below the threshold and then measures the voltage every 10 ms for 50 ms. Then it takes a vote to decide if the majority of readings are below the threshold. If so, it alarms. After 7 months, I had a false alarm.

Version 1.2.1 detects when a single reading is below the threshold and then measures the voltage every 10 ms for 1000 ms. Then it takes a vote to decide if the majority of readings are below the threshold. If so, it alarms. 
