import json
import logging
import os
from datetime import datetime

from dotenv import load_dotenv
from starlette.applications import Starlette
from starlette.requests import Request
from starlette.responses import JSONResponse, Response, PlainTextResponse
from starlette.routing import Route
from starlette.staticfiles import StaticFiles
from PIL import Image
import io
import hashlib
from telegram import InlineKeyboardMarkup, KeyboardButton, Update, InlineKeyboardButton, ReplyKeyboardMarkup 
from telegram.ext import (
    Application,
    CommandHandler,
    MessageHandler,
    CallbackQueryHandler,
    filters,
    ContextTypes,
    ConversationHandler,
)
from telegram.ext import (
    Application,
    CommandHandler,
    MessageHandler,
    CallbackQueryHandler,
    filters,
    ContextTypes,
    ConversationHandler,
)

WIFI_NAME, WIFI_PASSWORD = range(2)

# Load environment variables
load_dotenv()
TOKEN = os.getenv("TOKEN")
DOMAIN_NAME = os.getenv("DOMAIN_NAME")

# Texts from .env
WELCOME_TEXT = os.getenv("WELCOME_TEXT", "Welcome! Your user ID has been registered.")
NOTIFY_PROMPT = os.getenv(
    "NOTIFY_PROMPT", "Do you want to notify all users about this upload?"
)
UPDATE_TEXT = os.getenv("UPDATE_TEXT", "A new image has been uploaded!")
WIFI_CONNECT_TEXT = os.getenv(
    "WIFI_CONNECT_TEXT", "Thank you for sharing your network!"
)
# Load environment variables
ABOUT_TEXT = os.getenv("ABOUT_TEXT")

# Enable logging
logging.basicConfig(
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    level=logging.INFO,  # can change to DEBUG for more verbosity
)
logger = logging.getLogger(__name__)

# Directories for storing data
IMAGE_DIR = "./data"
os.makedirs(IMAGE_DIR, exist_ok=True)

USER_DATA_DIR = "./users"
os.makedirs(USER_DATA_DIR, exist_ok=True)
USER_DATA_FILE = os.path.join(USER_DATA_DIR, "user_data.json")
NETWORKS_FILE = os.path.join(USER_DATA_DIR, "networks.json")

# Create an empty JSON file for user data if it doesn't exist
if not os.path.exists(USER_DATA_FILE):
    with open(USER_DATA_FILE, "w") as file:
        json.dump([], file)

# Initialize the Telegram bot application
application = Application.builder().token(TOKEN).build()


async def start(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Handle the /start command and register the user, then show the main menu."""
    user_id = update.effective_user.id
    register_user(user_id)
    await update.message.reply_text(WELCOME_TEXT)
    await show_main_menu(update, context)  # Show the main menu after welcome


async def handle_photo(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """
    Handle photo uploads.
    After saving the photo, prompt the user to notify all other users.
    """
    user = update.effective_user
    photo = update.message.photo[-1]
    caption = update.message.caption or "image"

    # Build the filename: yy-mm-dd-hh-mm-ss-username-img_name.jpg
    now_str = datetime.now().strftime("%y-%m-%d-%H-%M-%S")
    username_str = user.username if user.username else str(user.id)
    sanitized_caption = caption.replace(" ", "_")
    filename = f"{now_str}-{username_str}-{sanitized_caption}.jpg"
    file_path = os.path.join(IMAGE_DIR, filename)

    # Download the file
    file_obj = await context.bot.get_file(photo.file_id)
    await file_obj.download_to_drive(file_path)

    # Prompt user whether to notify everyone
    keyboard = [
        [
            InlineKeyboardButton("Давай", callback_data="notify_yes"),
            InlineKeyboardButton("Ну не", callback_data="notify_no"),
        ]
    ]
    reply_markup = InlineKeyboardMarkup(keyboard)

    # Send the notification prompt
    await update.message.reply_text(NOTIFY_PROMPT, reply_markup=reply_markup)


async def button_handler(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Handle all button presses (InlineKeyboardButton callbacks)."""
    query = update.callback_query
    await query.answer()

    if query.data == "upload_image":
        await query.message.reply_text("Просто загрузи картинку.")

    elif query.data == "notify_yes":
        # Delete the original message with buttons
        await query.message.delete()

        # Broadcast the update text to all users
        user_ids = get_all_user_ids()
        for user_id in user_ids:
            try:
                await context.bot.send_message(chat_id=user_id, text=UPDATE_TEXT)
            except Exception as e:
                logger.error(f"Failed to send notification to {user_id}: {e}")

    elif query.data == "notify_no":
        # Delete the original message with buttons
        await query.message.delete()
        # await query.message.reply_text("Okay, I won't notify anyone.")

    # Removed the 'share_wifi' handling from here to let ConversationHandler manage it


async def handle_text(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Handle plain text messages (non-photo, non-command)."""
    user_text = update.message.text
    logger.info(f"User sent text message: {user_text}")
    await update.message.reply_text(f"You said: {user_text}")


def register_user(user_id: int) -> None:
    """Register a new user by adding their user ID to the JSON file."""
    with open(USER_DATA_FILE, "r") as file:
        user_ids = json.load(file)
    if user_id not in user_ids:
        user_ids.append(user_id)
        with open(USER_DATA_FILE, "w") as file:
            json.dump(user_ids, file)
        logger.info(f"Registered new user: {user_id}")
    else:
        logger.info(f"User {user_id} is already registered.")


def get_all_user_ids() -> list:
    """Retrieve all registered user IDs from the JSON file."""
    with open(USER_DATA_FILE, "r") as file:
        user_ids = json.load(file)
    return user_ids


async def telegram_webhook(request: Request) -> PlainTextResponse:
    """Webhook endpoint for Telegram updates."""
    # Log incoming request for debugging
    try:
        data = await request.json()
        logger.debug(f"Incoming update: {data}")
    except Exception as e:
        logger.error(f"Failed to parse incoming update: {e}")
        return PlainTextResponse("Error parsing JSON", status_code=400)

    # Process the update
    update = Update.de_json(data, application.bot)
    await application.process_update(update)
    return PlainTextResponse("OK")


async def get_last_img(request: Request) -> Response:
    """Serve the latest uploaded image."""
    try:
        # Get list of image files sorted by modification time in descending order
        image_files = sorted(
            (
                os.path.join(IMAGE_DIR, f)
                for f in os.listdir(IMAGE_DIR)
                if f.endswith(".jpg")
            ),
            key=os.path.getmtime,
            reverse=True,
        )
        if image_files:
            latest_image_path = image_files[0]

            # Open with Pillow
            img = Image.open(latest_image_path)

            # Resize to 256x122 (unproportional)
            img = img.resize((256, 122), Image.Resampling.NEAREST)

            # Convert to pure black & white (1-bit), no grayscale
            img = img.convert("1", dither=Image.Dither.NONE)

            img_byte_arr = io.BytesIO()
            img.save(img_byte_arr, format="JPEG")
            img_byte_arr.seek(0)
            return Response(content=img_byte_arr.read(), media_type="image/jpeg")
        else:
            return JSONResponse({"error": "No images found."}, status_code=404)
    except Exception as e:
        logger.error(f"Error serving the latest image: {e}")
        return JSONResponse({"error": "Internal server error."}, status_code=500)


async def get_last_xbm(request: Request) -> Response:
    """
    Return the latest uploaded image as an XBM text, after:
      1) Resizing (unproportionally) to 256x122
      2) Converting to pure black & white (no grayscale)
      3) Generating a valid XBM string
    """
    try:
        # Get list of .jpg files, newest first
        image_files = sorted(
            (
                os.path.join(IMAGE_DIR, f)
                for f in os.listdir(IMAGE_DIR)
                if f.endswith(".jpg")
            ),
            key=os.path.getmtime,
            reverse=True,
        )
        if not image_files:
            return JSONResponse({"error": "No images found."}, status_code=404)

        latest_image_path = image_files[0]

        # Open with Pillow
        img = Image.open(latest_image_path)

        # Resize to 256x122 (unproportional)
        img = img.resize((256, 122), Image.Resampling.NEAREST)

        # Convert to pure black & white (1-bit), no grayscale
        img = img.convert("1", dither=Image.Dither.NONE)

        # Build an XBM string manually
        xbm_str = generate_xbm_string(img)

        # Return as text
        return PlainTextResponse(xbm_str, media_type="text/plain")
    except Exception as e:
        logger.error(f"Error creating XBM: {e}")
        return JSONResponse({"error": "Internal server error."}, status_code=500)


def generate_xbm_string(img: Image.Image) -> str:
    """
    Generate a minimal XBM string from a 1-bit Pillow image.
    We'll assume the image is exactly 256x122 in '1' mode.
    """
    width, height = img.size
    pixels = img.load()

    # Build a list of hex values, 1 bit per pixel => 8 pixels per byte
    xbm_data = []
    byte_val = 0
    bit_index = 0

    # Pillow '1' mode => 0=Black, 255=White
    # Define 'black' as bit=1, 'white' as bit=0
    for y in range(height):
        for x in range(width):
            pixel_value = pixels[x, y]  # 0 or 255
            bit = 1 if pixel_value == 0 else 0
            byte_val |= bit << bit_index
            bit_index += 1
            if bit_index == 8:
                xbm_data.append(f"0x{byte_val:02x}")
                byte_val = 0
                bit_index = 0

    # If width*height is not a multiple of 8, flush the last partial byte
    if bit_index > 0:
        xbm_data.append(f"0x{byte_val:02x}")

    # Build the final text output
    xbm_str = f"#define wifi_width {width}\n"
    xbm_str += f"#define wifi_height {height}\n"
    xbm_str += "static const unsigned char wifi_bits[] = {\n  "
    xbm_str += ", ".join(xbm_data)
    xbm_str += "\n};\n"

    return xbm_str


async def get_last_md5(request: Request) -> JSONResponse:
    """
    Return the MD5 hash of the latest uploaded image file.
    """
    try:
        # Get the list of image files, sorted by modification time (newest first)
        image_files = sorted(
            (
                os.path.join(IMAGE_DIR, f)
                for f in os.listdir(IMAGE_DIR)
                if f.endswith(".jpg")
            ),
            key=os.path.getmtime,
            reverse=True,
        )
        if not image_files:
            return JSONResponse({"error": "No images found."}, status_code=404)

        latest_image_path = image_files[0]

        # Compute the MD5 hash of the file
        with open(latest_image_path, "rb") as file:
            file_hash = hashlib.md5(file.read()).hexdigest()

        return Response(file_hash)
    except Exception as e:
        logger.error(f"Error calculating MD5 hash: {e}")
        return JSONResponse({"error": "Internal server error."}, status_code=500)


async def get_wifi_book(request: Request) -> JSONResponse:
    """
    Serve the networks.json file as a JSON response.
    """
    try:
        if os.path.exists(NETWORKS_FILE):
            with open(NETWORKS_FILE, "r") as file:
                wifi_book = json.load(file)
            return JSONResponse(wifi_book)
        else:
            return JSONResponse({"error": "Wi-Fi book not found."}, status_code=404)
    except Exception as e:
        logger.error(f"Error serving Wi-Fi book: {e}")
        return JSONResponse({"error": "Internal server error."}, status_code=500)

async def share_wifi(update: Update, context: ContextTypes.DEFAULT_TYPE) -> int:
    """Initiate the process of sharing WiFi."""
    if update.callback_query:
        # If triggered from a button, acknowledge and edit the message
        await update.callback_query.answer()
        await update.callback_query.message.delete()
        await update.callback_query.message.reply_text("Введите имя сети WiFi:")
    elif update.message:
        # If triggered from a command or keyboard button
        await update.message.reply_text("Введите имя сети WiFi:")
    return WIFI_NAME


async def handle_wifi_name(update: Update, context: ContextTypes.DEFAULT_TYPE) -> int:
    """Handle WiFi name input."""
    context.user_data["wifi_name"] = update.message.text
    await update.message.reply_text("Теперь введите пароль для WiFi:")
    return WIFI_PASSWORD


async def handle_wifi_password(
    update: Update, context: ContextTypes.DEFAULT_TYPE
) -> int:
    """Handle WiFi password input."""
    wifi_name = context.user_data.get("wifi_name")
    wifi_password = update.message.text

    if not wifi_name:
        await update.message.reply_text("Произошла ошибка. Пожалуйста, начните снова.")
        return ConversationHandler.END

    # Save the WiFi network to networks.json
    try:
        if os.path.exists(NETWORKS_FILE):
            with open(NETWORKS_FILE, "r") as file:
                networks = json.load(file)
        else:
            networks = []

        # Add new network
        networks.append({"ssid": wifi_name, "password": wifi_password})

        # Write back to file
        with open(NETWORKS_FILE, "w") as file:
            json.dump(networks, file, indent=4)

        # Respond to the user
        await update.message.reply_text(
            f"WiFi '{wifi_name}' успешно добавлен.\n\n{WIFI_CONNECT_TEXT}"
        )

    except Exception as e:
        logger.error(f"Failed to save WiFi network: {e}")
        await update.message.reply_text(
            "Произошла ошибка при сохранении WiFi. Попробуйте ещё раз."
        )

    # Show the main menu again after completion
    await show_main_menu(update, context)
    return ConversationHandler.END


async def show_main_menu(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Show the main menu with available actions using ReplyKeyboardMarkup."""
    keyboard = [
        [KeyboardButton("Загрузить мем"), KeyboardButton("Добавить Wi-Fi")],
        [KeyboardButton("Об Анатолии Васильевиче")]
    ]
    reply_markup = ReplyKeyboardMarkup(keyboard, resize_keyboard=True)
    await update.message.reply_text("Выберите действие:", reply_markup=reply_markup)

        
async def help_command(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Send a message when the command /help is issued."""
    await update.message.reply_text(ABOUT_TEXT)


async def handle_main_menu_selection(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    """Handle user selections from the main menu."""
    user_selection = update.message.text.lower()

    if user_selection == "загрузить мем":
        await update.message.reply_text("Просто загрузи картинку как ты обычно делаешь в Телеграме.")
    elif user_selection == "добавить wi-fi":
        await share_wifi(update, context)
    elif user_selection == "об анатолии васильевиче":
        await help_command(update, context)
    else:
        await update.message.reply_text(
            "Не понял, попробуй еще раз."
        )


# Define the routes for the Starlette application
routes = [
    Route("/telegram", endpoint=telegram_webhook, methods=["POST"]),
    Route("/get_last_img", endpoint=get_last_img, methods=["GET"]),
    Route("/get_last_xbm", endpoint=get_last_xbm, methods=["GET"]),
    Route("/get_last_md5", endpoint=get_last_md5, methods=["GET"]),
    Route("/get_wifi_book", endpoint=get_wifi_book, methods=["GET"]),
]

# Initialize the Starlette application
app = Starlette(routes=routes)


async def on_startup() -> None:
    """Configure handlers and set webhook on startup."""
    
    # 1) Add all your handlers

    # Add the ConversationHandler first to give it higher priority
    wifi_conversation_handler = ConversationHandler(
        entry_points=[
            CommandHandler("share_wifi", share_wifi),
            CallbackQueryHandler(share_wifi, pattern="share_wifi"),
            MessageHandler(filters.Regex("^Share WiFi$"), share_wifi),  # Handle keyboard button
        ],
        states={
            WIFI_NAME: [
                MessageHandler(filters.TEXT & ~filters.COMMAND, handle_wifi_name)
            ],
            WIFI_PASSWORD: [
                MessageHandler(filters.TEXT & ~filters.COMMAND, handle_wifi_password)
            ],
        },
        fallbacks=[
            CommandHandler("cancel", cancel_conversation)
        ],
        allow_reentry=True,
    )
    application.add_handler(wifi_conversation_handler)
    
    # Add CommandHandler for /start
    application.add_handler(CommandHandler("start", start))
    
    # Add MessageHandler for photos
    application.add_handler(MessageHandler(filters.PHOTO, handle_photo))
    
    # Add CallbackQueryHandler for other buttons
    application.add_handler(CallbackQueryHandler(button_handler, pattern="^(?!share_wifi).*"))  # Exclude 'share_wifi' to avoid conflict
    
    application.add_handler(CommandHandler("help", help_command))

    # Add the generic MessageHandler last
    application.add_handler(MessageHandler(filters.TEXT & ~filters.COMMAND, handle_main_menu_selection))
    
    # 2) Initialize the PTB Application
    await application.initialize()

    # 3) Start the PTB Application (important for receiving updates)
    await application.start()

    # 4) Set the webhook AFTER starting the application
    webhook_url = f"{DOMAIN_NAME}/telegram"
    await application.bot.set_webhook(webhook_url)
    logger.info(f"Webhook set to {webhook_url}")


async def on_shutdown() -> None:
    """Shutdown the PTB Application and remove webhook."""
    await application.bot.delete_webhook()
    await application.stop()
    logger.info("Webhook removed")


async def cancel_conversation(update: Update, context: ContextTypes.DEFAULT_TYPE) -> int:
    """Handle cancellation of the conversation."""
    if update.callback_query:
        await update.callback_query.answer()
        await update.callback_query.message.reply_text("Разговор отменён.")
    else:
        await update.message.reply_text("Разговор отменён.")
    return ConversationHandler.END


app.add_event_handler("startup", on_startup)
app.add_event_handler("shutdown", on_shutdown)

# Mount the static files directory to serve images
app.mount("/data", StaticFiles(directory=IMAGE_DIR), name="data")

if __name__ == "__main__":
    import uvicorn

    # Ensure you run on HTTPS if you’re exposing this publicly,
    # or use an HTTPS reverse proxy in front of this app.
    uvicorn.run(app, host="0.0.0.0", port=7462)
