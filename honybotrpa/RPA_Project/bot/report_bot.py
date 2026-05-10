import os
import csv
from playwright.sync_api import sync_playwright

BASE_DIR = os.path.dirname(os.path.dirname(__file__))
CSV_PATH = os.path.join(BASE_DIR, "backend", "attacks.csv")


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

    page.wait_for_selector(".inventory_item")


def main():

    with sync_playwright() as p:

        browser = p.chromium.launch(headless=False)

        page = browser.new_page()

        login(page)

        with open(CSV_PATH, newline="", encoding="utf-8") as file:

            reader = csv.DictReader(file)

            for row in reader:

                ip = row["ip"]

                send_report(page, ip)

        input("Press Enter to close...")
        browser.close()


main()
