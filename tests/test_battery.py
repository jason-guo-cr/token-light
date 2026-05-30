import unittest

from token_light.battery import parse_pmset_battery


class BatteryTests(unittest.TestCase):
    def test_parse_pmset_battery_percent_and_state(self):
        output = """Now drawing from 'Battery Power'
 -InternalBattery-0 (id=5439587)\t61%; discharging; 3:28 remaining present: true
"""

        battery = parse_pmset_battery(output)

        self.assertEqual(battery["percent"], 61)
        self.assertFalse(battery["charging"])
        self.assertEqual(battery["state"], "discharging")

    def test_parse_pmset_charging(self):
        output = """Now drawing from 'AC Power'
 -InternalBattery-0 (id=5439587)\t82%; charging; 1:04 remaining present: true
"""

        battery = parse_pmset_battery(output)

        self.assertEqual(battery["percent"], 82)
        self.assertTrue(battery["charging"])

    def test_missing_battery_returns_none(self):
        self.assertIsNone(parse_pmset_battery("No batteries are installed."))


if __name__ == "__main__":
    unittest.main()
