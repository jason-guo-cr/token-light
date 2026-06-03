import unittest

from token_light.weather import WeatherParseError, parse_weather


class WeatherTests(unittest.TestCase):
    def test_parse_weather_formats_compact_display(self):
        payload = {
            "current": {
                "temperature_2m": 23.6,
                "weather_code": 1,
            }
        }

        weather = parse_weather(payload, "BJ")

        self.assertEqual(weather["display"], "BJ 24C")
        self.assertEqual(weather["temperature_c"], 24)
        self.assertEqual(weather["condition"], "cloud")

    def test_parse_weather_rejects_missing_current(self):
        with self.assertRaises(WeatherParseError):
            parse_weather({}, "BJ")


if __name__ == "__main__":
    unittest.main()
