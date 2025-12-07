from gtts import gTTS
import os

def text_to_speech_thai(file_path):
    # 1. Read the text from your file
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            my_text = f.read()
    except FileNotFoundError:
        print("Error: File not found.")
        return

    if not my_text.strip():
        print("Error: The file is empty.")
        return

    print("Converting text to speech... (this requires internet)")
    
    # 2. Convert to audio (lang='th' sets it to Thai)
    # slow=False makes it speak at normal speed
    tts = gTTS(text=my_text, lang='th', slow=False)
    
    # 3. Save the audio file
    output_file = "output_thai_pija.mp3"
    tts.save(output_file)
    
    print(f"Success! Saved as {output_file}")
    
    # 4. Automatically play the file (Works on Windows/Mac)
    if os.name == 'nt': # Windows
        os.system(f"start {output_file}")
    else: # Mac/Linux
        os.system(f"open {output_file}")

# Usage: Create a file named 'data.txt' with Thai text in the same folder
# Then run this script.
if __name__ == "__main__":
    # Create a dummy file for testing if it doesn't exist
    if not os.path.exists("data.txt"):
        with open("data.txt", "w", encoding="utf-8") as f:
            f.write("สวัสดีครับ วันนี้อากาศดีมาก")
            
    text_to_speech_thai("data.txt")