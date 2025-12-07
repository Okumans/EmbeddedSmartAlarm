import time

try:
    import torch
    _TORCH_AVAILABLE = True
except Exception:
    _TORCH_AVAILABLE = False

try:
    # faster-whisper provides a much faster CUDA path (float16)
    from faster_whisper import WhisperModel
    _FAST_WHISPER_AVAILABLE = True
except Exception:
    _FAST_WHISPER_AVAILABLE = False
    import whisper


def _default_device():
    if _TORCH_AVAILABLE and torch.cuda.is_available():
        return "cuda"
    return "cpu"


def transcribe_audio(audio_file, model_size="base", device=None):
    """Transcribe `audio_file` using faster-whisper if available, otherwise fall back to OpenAI's whisper.

    Returns the concatenated text.
    """
    device = device or _default_device()
    print(f"Using device: {device}")

    print("Loading model...")
    t0 = time.time()

    if _FAST_WHISPER_AVAILABLE:
        model = WhisperModel(model_size, device=device, compute_type="float16")
        load_time = time.time() - t0
        print(f"Model load time (faster-whisper): {load_time:.2f}s")

        print("Transcribing with faster-whisper...")
        t0 = time.time()
        segments, info = model.transcribe(audio_file, beam_size=5)
        trans_time = time.time() - t0
        print(f"Transcription time (faster-whisper): {trans_time:.2f}s")

        # segments may be a generator; collect text
        text = "".join([segment.text for segment in segments])
        # info contains language and other metadata
        print("Detected language:", getattr(info, "language", None))
        return text
    else:
        print("faster-whisper not available — falling back to OpenAI's whisper (slower).")
        model = whisper.load_model(model_size, device=device)
        load_time = time.time() - t0
        print(f"Model load time (whisper): {load_time:.2f}s")

        print("Transcribing with whisper...")
        t0 = time.time()
        result = model.transcribe(audio_file)
        trans_time = time.time() - t0
        print(f"Transcription time (whisper): {trans_time:.2f}s")
        return result.get("text", "")


if __name__ == "__main__":
    input_file = "test.mp3"  # your input file
    model_size = "base"

    device = "cuda" if (_TORCH_AVAILABLE and torch.cuda.is_available()) else "cpu"

    before_benchmark = time.time()
    text = transcribe_audio(input_file, model_size=model_size, device=device)
    after_benchmark = time.time()

    print(f"Transcription took {after_benchmark - before_benchmark:.2f} seconds")
    print("\nTRANSCRIPTION RESULT:")
    print(text)
    

    print(f"Transcription took {after_benchmark - before_benchmark:.2f} seconds")
    print("\nTRANSCRIPTION RESULT:")
    print(text)
