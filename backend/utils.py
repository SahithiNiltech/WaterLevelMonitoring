from flask import request
from config import Config


def check_api_key():
    """Validate API key from query param or header."""
    api_key = request.args.get('api_key') or request.headers.get('X-API-Key')
    return api_key == Config.API_KEY
