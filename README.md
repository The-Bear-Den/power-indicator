# Summary
This project is to great a LED indicator to show the current electricity usage.

# Home Assistant Configuration
There is a Home Assistant automation that will publish to the MQTT broker. The data is provided from
a Home Assistant services that pulls data from the Solar/Battery/Inverter.

```{yaml}
metadata: {}
data:
  qos: 0
  topic: /homeassistant/energy
  payload: |-
    {
      "time": "{{ now().isoformat() }}",
      "rows": [
        {
          "name": "HOUSE_LOAD",
          "type": "range",
          "value": {{ "%d" | format(states('sensor.qhm0h980cz_load_consumption') | float * 1000 | int) }},
          "upper_value": 6000
        },
        {
          "name": "GRID_EXPORT",
          "type": "range",
          "value": {{ "%d" | format(states('sensor.qhm0h980cz_export_to_grid') | float * 1000 | int) }},
          "upper_value": 10000
        },
        {
          "name": "GRID_IMPORT",
          "type": "range",
          "value": {{ "%d" | format(states('sensor.qhm0h980cz_import_from_grid') | float * 1000 | int) }},
          "upper_value": 3000
        },
        {
          "name": "SOC",
          "type": "percent",
          "value": {{ states('sensor.qhm0h980cz_statement_of_charge') }}
        },
        {
          "name": "PV1",
          "type": "range",
          "value": {{ "%d" | format(states('sensor.qhm0h980cz_pv1_wattage') | float * 1000 | int) }},
          "upper_value": 5000
        },
        {
          "name": "PV2",
          "type": "range",
          "value": {{ "%d" | format(states('sensor.qhm0h980cz_pv2_wattage') | float * 1000 | int) }},
          "upper_value": 5000
        }
      ]
    }
action: mqtt.publish
```
