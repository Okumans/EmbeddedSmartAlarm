from google import genai
import time
import random

from string import Template

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
Examples of Unique Output:

"{ดาวเคราะห์ดวงใดในระบบสุริยะที่มีดวงจันทร์ชื่อ ไททัน?}" (Target: Saturn/เสาร์)
"{ในภาษาคอมพิวเตอร์ 1 ไบต์ มีกี่บิต?}" (Target: 8)
"{สัตว์ชนิดใดที่มีหัวใจ 3 ห้องและเลือดยังไม่แยก Oxygen ชัดเจน?}" (Target: Frog/Gop or Amphibian)
"{ผลรวมของมุมภายในรูปหกเหลี่ยมคือเท่าไหร่?}" (Target: 720)
"""

VALIDATION_PROMPT = Template(
    """
    You are a "Wake-Up Challenge" validator. Given the trivia question and its answer, determine if the answer is valid
    Question: $question
    Answer: $answer
    Answer Format: Output strictly only "Valid" or "Invalid" inside double quotes.
    """
)

client = genai.Client(api_key=API_KEY)


def generate_with_retry(model, contents, initial_backoff=1.0, multiplier=2.0, max_backoff=60.0):
    """Call the Gemini model and retry until success using exponential backoff with jitter.

    - initial_backoff: starting sleep (seconds)
    - multiplier: backoff multiplier applied after each failure
    - max_backoff: maximum sleep between attempts
    This function retries indefinitely until a successful response is returned.
    """
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
            # Exponential backoff with small random jitter to avoid thundering herd
            jitter = random.uniform(0, 0.5 * backoff)
            sleep_time = min(backoff + jitter, max_backoff)
            print(f"Request failed (attempt {attempt}): {e}. Retrying in {sleep_time:.1f}s...", flush=True)
            time.sleep(sleep_time)
            backoff = min(backoff * multiplier, max_backoff)


response = generate_with_retry(
    model="gemini-2.5-flash-lite",
    contents=QUESTION_PROPMT,
)


def validate_answer(answer, question=None, model="gemini-2.5-flash-lite"):
    """Validate an `answer` for a given `question` using the Gemini model.

    - `answer`: string with the user's answer (STT-friendly expected)
    - `question`: optional; if omitted, uses the last generated question stored in
      `question_text` (if available). If neither is available, raises ValueError.
    - returns: the raw Gemini response object from `generate_with_retry`.
    """
    if question is None:
        if 'question_text' not in globals() or not question_text:
            raise ValueError('No question available to validate; pass `question` explicitly')
        question = question_text

    prompt = VALIDATION_PROMPT.substitute(question=question, answer=answer)
    validation_response = generate_with_retry(
        model=model,
        contents=prompt,
    )
    return validation_response


# Store the generated question so it can be validated later
question_text = response.text.strip('"{} \n')
print(question_text)

# Example usage (uncomment to run):
validation = validate_answer(input("Your answer: "), question=question_text)
print('Validation result:', validation.text.strip())