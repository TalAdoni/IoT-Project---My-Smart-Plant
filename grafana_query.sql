SELECT
  timestamp AS time,
  plant,
  -- (2483=0%, 1100=100%)New(2600=0%, 995=100%)
  CASE 
    WHEN soilRaw >= 2600 THEN 0
    WHEN soilRaw <= 995 THEN 100
    ELSE ((2600.0 - soilRaw) / 16.05) 
  END AS "soil moisture %",
  tempC AS "temperature",
  humRH AS "air humidity"
FROM sensor_data
WHERE $__timeFilter(timestamp)
ORDER BY 1 ASC;