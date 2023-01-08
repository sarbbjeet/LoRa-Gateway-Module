## LoRa-Gateway-Module
This is Arduino IDE code for connecting the NodeMcu to the LoRa transceiver and MQTT broker.
This is a hardware component of an agriculture monitoring system that acts as a gateway, converting LoRa radio signals to MQTT json data.
Check out the repositories listed below to set up a complete smart agricultural monitoring system.
1. LoRa Node Module  https://github.com/sarbbjeet/LoRa-Node-Module 
2. LoRa Gateway Module https://github.com/sarbbjeet/LoRa-Gateway-Module
3. Cloud Server https://github.com/sarbbjeet/Agro-server
4. Mobile App https://github.com/sarbbjeet/Agro-monitoring-app


## Architecture of the project 
The image below depicts the overall architecture of the project, which includes sensor units, a gateway module, a cloud server, and a front user interface application.
This project features LoRa communication, MQTT client/broker interaction, and database queries communicating with JSON API.


<img width="716" alt="Screenshot 2023-01-04 at 10 40 32" src="https://user-images.githubusercontent.com/9445093/211218105-fccd1078-afa1-4744-a16c-c1dd53dca03f.png">



## Circuit diagram and pinout 
<div>
<img src="https://user-images.githubusercontent.com/9445093/211204315-8b8ad0b5-50f1-4df3-b28f-93b959d527ee.jpg" width="300" height="450"> 
<img src="https://user-images.githubusercontent.com/9445093/211204672-922aaf1e-18c4-45a8-816c-1bd70f330496.png" width="350" height="450"> 
<img src="https://user-images.githubusercontent.com/9445093/211204380-8417683c-5197-4ce3-9d2e-45794c0948ff.png" width="350" height="300"> 
</div>

## JSON data format
This JSON data format helps to publish and subscribe data from MQTT broker. 
1. Publish data to MQTT broker 
``` \outTopic/farmer_id ``` To publish data to a specific farmer ID, the given topic requires farmer id.
This farmer id is retrieved via the android app Gateway setup page during Gateway Module network configuration.

Published JSON data format is listed below 
```
{"gateway":170, "node": 21,"deviceModel": 0 ,"data": {"sensor0": 4.12, "sensor1": 65, "relay0": 0}}
```
2. Subscribe data to MQTT broker 
``` \inTopic/farmer_id ``` To publish data to a specific farmer ID, the given topic requires farmer id.
This farmer id is retrieved via the android app Gateway setup page during Gateway Module network configuration.

Subscribed JSON data format is listed below 
```
{"gateway":170, "node": 21,"deviceModel": 0 ,"data": {"relay0": 0}}
```
