import importlib.util
from pathlib import Path
from types import ModuleType

GEN_IDS = Path(__file__).resolve().parents[2] / "tools/gen_ids.py"


def load_gen_ids() -> ModuleType:
    spec = importlib.util.spec_from_file_location("gen_ids", GEN_IDS)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_generated_headers_up_to_date() -> None:
    """Fails if protocol/ changed without re-running `uv run python tools/gen_ids.py`."""
    for path, text in load_gen_ids().render().items():
        assert path.read_text() == text, f"{path.name} is stale: run tools/gen_ids.py"
