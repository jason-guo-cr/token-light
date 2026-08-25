from __future__ import annotations

import json
from pathlib import Path


DEFAULT_AUTH_FILE = Path.home() / ".codex" / "auth.json"


class AuthError(RuntimeError):
    pass


def read_access_token(auth_file: Path | str = DEFAULT_AUTH_FILE) -> str:
    path = Path(auth_file).expanduser()
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise AuthError(f"Codex auth file not found: {path}") from exc
    except json.JSONDecodeError as exc:
        raise AuthError(f"Codex auth file is not valid JSON: {path}") from exc

    if not isinstance(payload, dict):
        raise AuthError(f"Codex auth file is not a valid JSON object: {path}")

    tokens = payload.get("tokens")
    if not isinstance(tokens, dict):
        raise AuthError(f"Codex auth file tokens field is not an object: {path}")

    token = tokens.get("access_token")
    if not isinstance(token, str) or not token:
        raise AuthError(f"Codex auth file does not contain an access token: {path}")
    return token
