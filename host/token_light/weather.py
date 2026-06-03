from __future__ import annotations

import json
import socket
import urllib.error
import urllib.parse
import urllib.request
from typing import Any


WEATHER_URL = "https://api.open-meteo.com/v1/forecast"


class WeatherFetchError(RuntimeError):
    pass


class WeatherParseError(RuntimeError):
    pass


def _condition_for_code(code: int) -> str:
    if code == 0:
        return "clear"
    if code in {1, 2, 3, 45, 48}:
        return "cloud"
    if code in {51, 53, 55, 56, 57, 61, 63, 65, 66, 67, 80, 81, 82}:
        return "rain"
    if code in {71, 73, 75, 77, 85, 86}:
        return "snow"
    if code in {95, 96, 99}:
        return "storm"
    return "weather"


def parse_weather(payload: Any, label: str) -> dict:
    if not isinstance(payload, dict):
        raise WeatherParseError("weather response is not a JSON object")
    current = payload.get("current")
    if not isinstance(current, dict):
        raise WeatherParseError("current weather is missing or invalid")

    try:
        temperature = float(current["temperature_2m"])
        weather_code = int(current.get("weather_code", -1))
    except KeyError as exc:
        raise WeatherParseError(f"current.{exc.args[0]} is missing") from exc
    except (TypeError, ValueError) as exc:
        raise WeatherParseError("current weather contains invalid values") from exc

    rounded = round(temperature)
    return {
        "label": label,
        "temperature_c": rounded,
        "weather_code": weather_code,
        "condition": _condition_for_code(weather_code),
        "display": f"{label} {rounded}C",
    }


def fetch_weather(latitude: float, longitude: float, label: str, timeout: float = 10.0) -> dict:
    params = urllib.parse.urlencode(
        {
            "latitude": latitude,
            "longitude": longitude,
            "current": "temperature_2m,weather_code",
            "timezone": "Asia/Shanghai",
        }
    )
    request = urllib.request.Request(f"{WEATHER_URL}?{params}", headers={"User-Agent": "token-light/0.1"})

    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            payload = json.loads(response.read())
    except (urllib.error.URLError, TimeoutError, socket.timeout) as exc:
        raise WeatherFetchError(f"Failed to fetch weather: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise WeatherFetchError("weather response is not valid JSON") from exc

    try:
        return parse_weather(payload, label)
    except WeatherParseError as exc:
        raise WeatherFetchError(str(exc)) from exc
