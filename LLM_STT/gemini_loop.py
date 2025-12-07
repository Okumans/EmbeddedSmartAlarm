from google import genai
import time
import random
from string import Template

# ---------------------------------------------------------
# 1. SETUP
# ---------------------------------------------------------

# PASTE YOUR KEY HERE
API_KEY = 'AIzaSyDOX89lRp_WZr_mjGyx4viRIIcZXF_Y-s8' 

QUESTION_PROPMT = """
Act as a "Wake-Up Challenge" generator. Generate a unique, difficult trivia question in Thai.
Strict Constraints:
High Variety Topics: Do NOT ask about common history dates or mountain heights. Instead, select a random category from: Astronomy, Human Anatomy, Thai Literature, Physics, Video Games, Chemical Elements, or Geometry.
The Answer: Must be STT-Friendly. The answer must be either:
A Number (e.g., speed of light, number of bones).
A Single Distinct Noun (e.g., name of a planet, a specific color, an animal, or a chemical element name).
Difficulty: The user must not be able to answer while half-asleep. They should need to Google it or calculate it.
Format: Output strictly only the question in Thai inside double quotes.
"""

VALIDATION_PROMPT = Template(
    """
    You are a "Wake-Up Challenge" validator. Given the trivia question and its answer, determine if the answer is valid
    Question: $question
    Answer: $answer
    Answer Format: Output strictly only "Valid" or "Invalid" inside double quotes.
    """
)

# New prompt to get the answer if user fails
REVEAL_ANSWER_PROMPT = Template(
    """
    What is the correct short answer to this trivia question?
    Question: $question
    Format: Just the answer text in Thai.
    """
)

client = genai.Client(api_key=API_KEY)

# ---------------------------------------------------------
# 2. HELPER FUNCTIONS
# ---------------------------------------------------------

def generate_with_retry(model, contents, initial_backoff=1.0, multiplier=2.0, max_backoff=60.0):
    attempt = 0
    backoff = float(initial_backoff)
    while True:
        attempt += 1
        try:
            response = client.models.generate_content(
                model=model,
                contents=contents,
            )
            return response
        except Exception as e:
            jitter = random.uniform(0, 0.5 * backoff)
            sleep_time = min(backoff + jitter, max_backoff)
            print(f"Request failed (attempt {attempt}): {e}. Retrying in {sleep_time:.1f}s...", flush=True)
            time.sleep(sleep_time)
            backoff = min(backoff * multiplier, max_backoff)

def validate_answer(answer, question, model="gemini-2.5-flash"):
    prompt = VALIDATION_PROMPT.substitute(question=question, answer=answer)
    validation_response = generate_with_retry(model=model, contents=prompt)
    # Clean the output (remove quotes or spaces)
    return validation_response.text.strip().replace('"', '')

def get_correct_answer(question, model="gemini-2.5-flash"):
    prompt = REVEAL_ANSWER_PROMPT.substitute(question=question)
    response = generate_with_retry(model=model, contents=prompt)
    return response.text.strip()

# ---------------------------------------------------------
# 3. MAIN LOGIC (The Loop)
# ---------------------------------------------------------

def run_challenge():
    print("Generating question... Please wait.")
    
    # 1. Generate Question
    response = generate_with_retry(
        model="gemini-2.5-flash-lite",
        contents=QUESTION_PROPMT,
    )
    
    question_text = response.text.strip('"{} \n')
    print(f"\nQUERY: {question_text}")

    # 2. Loop for 3 attempts
    max_attempts = 3
    
    for attempt in range(1, max_attempts + 1):
        user_input = input(f"\nAttempt {attempt}/{max_attempts} - Your Answer: ")
        
        # Check if input is empty
        if not user_input.strip():
            print("Please type an answer.")
            continue

        print("Validating...")
        result_status = validate_answer(user_input, question_text)

        if "Valid" in result_status and "Invalid" not in result_status:
            print(">>> CORRECT! Alarm Deactivated. Good Morning.")
            return # Exit function successfully
        else:
            print(">>> WRONG answer.")

    # 3. If loop finishes without success (Game Over)
    print("\n-----------------------------")
    print("FAILED 3 ATTEMPTS. GAME OVER.")
    
    real_answer = get_correct_answer(question_text)
    print(f"The correct answer was: {real_answer}")
    print("-----------------------------")

if __name__ == "__main__":
    run_challenge()