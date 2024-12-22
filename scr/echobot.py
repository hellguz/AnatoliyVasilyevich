import json
import logging
import os
from dotenv import load_dotenv
from telegram import Update, InlineKeyboardButton, InlineKeyboardMarkup, InputFile
from telegram.ext import (
    Application,
    CommandHandler,
    MessageHandler,
    CallbackQueryHandler,
    filters,
    ContextTypes,
)

# Enable logging
logging.basicConfig(
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s", level=logging.INFO
)
logger = logging.getLogger(__name__)

load_dotenv()

TOKEN = os.getenv("TOKEN")

# Directory to save uploaded images
IMAGE_DIR = "./data"

# Ensure the directory exists
os.makedirs(IMAGE_DIR, exist_ok=True)

# File to store user data
USER_DATA_FILE = "user_data.json"

# Ensure the user data file exists
if not os.path.exists(USER_DATA_FILE):
    with open(USER_DATA_FILE, "w") as file:
        json.dump([], file)


async def button_handler(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Handle button presses."""
    query = update.callback_query
    await query.answer()

    if query.data == "upload_image":
        await query.message.reply_text("Please upload an image.")


async def handle_photo(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Handle photo uploads."""
    photo = update.message.photo[-1]
    file = await context.bot.get_file(photo.file_id)
    file_path = os.path.join(IMAGE_DIR, f"{photo.file_id}.jpg")
    await file.download_to_drive(file_path)
    await update.message.reply_text("Image received and saved.")

    # Broadcast the image to all users
    # Note: Implement a method to keep track of user IDs
    for user_id in get_all_user_ids():
        try:
            with open(file_path, "rb") as img:
                await context.bot.send_photo(chat_id=user_id, photo=InputFile(img))
        except Exception as e:
            logger.error(f"Failed to send image to {user_id}: {e}")


async def start(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Handle the /start command and register the user."""
    user_id = update.effective_user.id
    await update.message.reply_text("Welcome! Your user ID has been registered.")
    register_user(user_id)


def register_user(user_id: int) -> None:
    """Register a new user by adding their user ID to the JSON file."""
    with open(USER_DATA_FILE, "r") as file:
        user_ids = json.load(file)
    if user_id not in user_ids:
        user_ids.append(user_id)
        with open(USER_DATA_FILE, "w") as file:
            json.dump(user_ids, file)


def get_all_user_ids() -> list:
    """Retrieve all registered user IDs from the JSON file."""
    with open(USER_DATA_FILE, "r") as file:
        user_ids = json.load(file)
    return user_ids


def main() -> None:
    """Start the bot."""
    application = Application.builder().token(TOKEN).build()

    application.add_handler(CommandHandler("start", start))
    application.add_handler(CallbackQueryHandler(button_handler))
    application.add_handler(MessageHandler(filters.PHOTO, handle_photo))

    application.run_polling()


if __name__ == "__main__":
    main()
