# IOT-5
## 5. Semester IOT projekt using Particle Argon

#### Use the link for the Particle webhook API call
https://api.energidataservice.dk/dataset/Elspotprices?offset=0&start={{{year}}}-{{{month}}}-{{{day}}}T{{{hour}}}:00&end={{{year}}}-{{{month}}}-{{{day_two}}}T00:00&filter=%7B%22PriceArea%22:%22DK2%22%7D&sort=HourUTC%20ASC&timezone=dk

#### Response template 
In Particle Webhook setup, use the following response template to extract the data needed:
***{{#records}}!{{HourDK}},{{SpotPriceDKK}}{{/records}}***
