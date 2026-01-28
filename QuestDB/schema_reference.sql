TABLE sensor_data (
  plant SYMBOL,         -- Plant Identifier
  soilRaw LONG,         -- Sensor Raw Value
  tempC DOUBLE,         -- Temperature
  humRH DOUBLE,         -- Humidity
  timestamp TIMESTAMP   -- Time of ingestion
)