#don't use this code
from gpt4all import GPT4All

# Load model (offline)
model = GPT4All("gpt4all-lora-quantized.bin")  # path to your .bin file

# Create a simple class with .generate() method
class LLM:
    def __init__(self, model):
        self.model = model

    def generate(self, prompt):
        response = self.model.generate(prompt, max_tokens=256)
        return response

# Instantiate LLM
LLM_instance = LLM(model)

# Example usage
prompt = "Explain quantum physics in simple words."
answer = LLM_instance.generate(prompt)
print(answer)
