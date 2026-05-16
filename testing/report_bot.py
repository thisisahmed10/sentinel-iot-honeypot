import os
import json
import urllib.request
from playwright.sync_api import sync_playwright

BACKEND_URL = os.environ.get("BACKEND_URL", "http://127.0.0.1:5000/logs")

def login(page):
    page.goto("https://www.saucedemo.com")
    page.wait_for_selector("#user-name")
    page.fill("#user-name", "standard_user")
    page.fill("#password", "secret_sauce")
    page.click("#login-button")
    page.wait_for_selector(".title")
    print("Login success")

def send_report(page, ip):
    print(f"Processing IP: {ip}")
    page.wait_for_selector(".inventory_item")
    page.locator(".inventory_item").first.locator("button").click()
    print(f"Action completed for {ip}")

def load_events():
    try:
        with urllib.request.urlopen(BACKEND_URL, timeout=5) as response:
            payload = response.read().decode("utf-8")
    except OSError as error:
        print(f"Failed to load backend events: {error}")
        return []
    data = json.loads(payload)
    return data if isinstance(data, list) else []

def main():
    events = load_events()
    if not events:
        print("No backend events available.")
        return

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=False)
        page = browser.new_page()
        login(page)
        seen = set()
        for event in events:
            ip = event.get("src_ip") or event.get("ip")
            if not ip or ip in seen:
                continue
            seen.add(ip)
            send_report(page, ip)
        input("Press Enter to close...")
        browser.close()

if __name__ == "__main__":
    main()
