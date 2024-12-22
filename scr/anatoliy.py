import json
import logging
import os
from dotenv import load_dotenv
from starlette.applications import Starlette
from starlette.requests import Request
from starlette.responses import JSONResponse, Response, PlainTextResponse
from starlette.routing import Route
from starlette.staticfiles import StaticFiles
from telegram import Update, InlineKeyboardButton, InlineKeyboardMarkup, InputFile
from telegram.ext import (
    Application,
    CommandHandler,
    MessageHandler,
    CallbackQueryHandler,
    filters,
    ContextTypes,
)

# Load environment variables
load_dotenv()
TOKEN = os.getenv("TOKEN")
DOMAIN_NAME = os.getenv("DOMAIN_NAME")

# Enable logging
logging.basicConfig(
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s", level=logging.INFO
)
logger = logging.getLogger(__name__)

# Directory to save uploaded images
IMAGE_DIR = "./data"
os.makedirs(IMAGE_DIR, exist_ok=True)

# File to store user data
USER_DATA_FILE = "./users/user_data.json"
if not os.path.exists(USER_DATA_FILE):
    with open(USER_DATA_FILE, "w") as file:
        json.dump([], file)

# Initialize the Telegram bot application
application = Application.builder().token(TOKEN).build()

async def start(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Handle the /start command and register the user."""
    user_id = update.effective_user.id
    await update.message.reply_text("Welcome! Your user ID has been registered.")
    register_user(user_id)

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
    for user_id in get_all_user_ids():
        try:
            with open(file_path, "rb") as img:
                await context.bot.send_photo(chat_id=user_id, photo=InputFile(img))
        except Exception as e:
            logger.error(f"Failed to send image to {user_id}: {e}")

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

async def telegram_webhook(request: Request) -> PlainTextResponse:
    """Handle incoming Telegram updates."""
    data = await request.json()
    update = Update.de_json(data, application.bot)
    await application.update_queue.put(update)
    return PlainTextResponse("OK")

async def get_last_img(request: Request) -> Response:
    """Serve the latest uploaded image."""
    try:
        # Get list of image files sorted by modification time in descending order
        image_files = sorted(
            (os.path.join(IMAGE_DIR, f) for f in os.listdir(IMAGE_DIR) if f.endswith(".jpg")),
            key=os.path.getmtime,
            reverse=True
        )
        if image_files:
            latest_image_path = image_files[0]
            return Response(
                content=open(latest_image_path, "rb").read(),
                media_type="image/jpeg"
            )
        else:
            return JSONResponse({"error": "No images found."}, status_code=404)
    except Exception as e:
        logger.error(f"Error serving the latest image: {e}")
        return JSONResponse({"error": "Internal server error."}, status_code=500)

# Define the routes for the Starlette application
routes = [
    Route("/telegram", endpoint=telegram_webhook, methods=["POST"]),
    Route("/get_last_img", endpoint=get_last_img, methods=["GET"]),
]

# Initialize the Starlette application
app = Starlette(routes=routes)

async def on_startup() -> None:
    """Set the webhook for Telegram bot on startup."""
    webhook_url = f"{DOMAIN_NAME}/telegram"
    await application.bot.set_webhook(webhook_url)
    logger.info(f"Webhook set to {webhook_url}")

async def on_shutdown() -> None:
    """Remove the webhook on shutdown."""
    await application.bot.delete_webhook()
    logger.info("Webhook removed")

app.add_event_handler("startup", on_startup)
app.add_event_handler("shutdown", on_shutdown)

# Mount the static files directory to serve images
app.mount("/data", StaticFiles(directory=IMAGE_DIR), name="data")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
