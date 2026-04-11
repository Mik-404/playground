# conftest.py
import pytest
import pygame
from unittest.mock import MagicMock, PropertyMock, patch
import logging
import os


# --- Pygame Initialization Fixture ---
@pytest.fixture(scope="session", autouse=True)
def pygame_init_session():
    """Initialize Pygame once per test session using a dummy driver."""
    os.environ["SDL_VIDEODRIVER"] = "dummy"
    try:
        pygame.init()
        pygame.display.init()
        pygame.display.set_mode((1, 1))
        pygame.font.init()
    except pygame.error as e:
        print(f"Warning: Pygame initialization failed: {e}")
        pytest.skip("Pygame initialization failed.", allow_module_level=True)
    yield
    pygame.quit()


# --- Pygame Event Mocking ---
@pytest.fixture
def mock_pygame_event():
    """Fixture to create mock Pygame events easily (using MagicMock)."""

    def _create_event(etype, **kwargs):
        attributes = {
            "type": etype,
            "pos": (-1, -1),
            "button": 0,
            "key": 0,
            "mod": 0,
            "unicode": "",
            "y": 0,
            **kwargs,
        }
        event = MagicMock(name=f"Event_{etype}")
        event.type = etype
        for k, v in attributes.items():
            setattr(event, k, v)
        event.button = attributes.get("button", 0)
        return event

    return _create_event


@pytest.fixture(autouse=True)
def mock_pygame_mouse(mocker):
    """Mocks pygame.mouse functions (function scope)."""
    mock_get_pos = mocker.patch("pygame.mouse.get_pos", return_value=(100, 100))
    mock_get_pressed = mocker.patch("pygame.mouse.get_pressed", return_value=(0, 0, 0))
    return {"get_pos": mock_get_pos, "get_pressed": mock_get_pressed}


# --- Mock Pygame Font ---
@pytest.fixture
def mock_pygame_font(mocker):
    """Creates a mock Font instance. Does NOT patch pygame.font.Font globally."""
    mock_font = MagicMock(spec=pygame.font.Font, name="MockPygameFontInstance")
    mock_text_surface = MagicMock(spec=pygame.Surface, name="MockTextSurfaceFromFont")
    mock_text_surface.get_rect.return_value = pygame.Rect(0, 0, 50, 20)
    mock_font.render.return_value = mock_text_surface
    mocker.patch("pygame.font.init", return_value=None)
    return mock_font


# --- Mock StaticLoader (Использует mock_pygame_font) ---
@pytest.fixture
def mock_static_loader(mocker, mock_pygame_font):
    """Provides a mock StaticLoader object."""
    mock_loader_instance = MagicMock(name="MockStaticLoaderInstance")
    mock_logger = MagicMock(spec=logging.Logger, name="MockLogger")
    mock_loader_instance.getLogger.return_value = mock_logger

    # Palette Codes
    mock_palette = {
        (0, 0, 255): ("#0000FF", "blue_code"),
        (128, 128, 128): ("#808080", "gray_code"),
        (255, 255, 0): ("#FFFF00", "yellow_code"),
        (0, 0, 0): ("#000000", "black_code"),
        "blue_code": (0, 0, 255),
        "gray_code": (128, 128, 128),
        "yellow_code": (255, 255, 0),
        "black_code": (0, 0, 0),
    }
    mock_loader_instance.getPaletteCodes.return_value = mock_palette

    # Icons
    mock_icon_surface = MagicMock(spec=pygame.Surface, name="MockIconSurface")
    mock_icon_surface.get_rect.return_value = pygame.Rect(0, 0, 18, 18)
    mock_loader_instance.getDragIcon.return_value = mock_icon_surface
    mock_loader_instance.getBrushIcon.return_value = mock_icon_surface

    mock_loader_instance.getDefaultFont.return_value = (
        mock_pygame_font
    )

    # --- Patching StaticLoader Usage ---
    target_module = "ai_life_simulator._front.front"
    # Патчим сам класс, т.к. UIElement может вызывать getLogger статически
    mocker.patch(f"{target_module}.StaticLoader", new=mock_loader_instance)
    return mock_loader_instance


# --- Generic Surface for Drawing ---
@pytest.fixture
def dummy_surface(mocker):
    """Provides a mock surface and mocks draw functions (function scope)."""
    surface = MagicMock(spec=pygame.Surface, name="DummyDrawSurface")
    surface.get_rect.return_value = pygame.Rect(0, 0, 800, 600)
    surface.get_clip.return_value = pygame.Rect(0, 0, 800, 600)
    surface.set_clip = MagicMock(name="MockSetClip")
    surface.blit = MagicMock(name="MockBlit")
    surface.fill = MagicMock(name="MockFill")

    draw_module = "pygame.draw"
    transform_module = "pygame.transform"
    mock_rect = mocker.patch(f"{draw_module}.rect")
    mock_circle = mocker.patch(f"{draw_module}.circle")
    # Мокаем smoothscale здесь, так как он используется в MapView.__init__
    mock_scaled_surface = MagicMock(spec=pygame.Surface, name="MockScaledSurfaceGlobal")
    mock_scaled_surface.get_rect.return_value = pygame.Rect(0, 0, 20, 20)
    mock_smoothscale = mocker.patch(
        f"{transform_module}.smoothscale", return_value=mock_scaled_surface
    )

    surface.mock_draw_rect = mock_rect
    surface.mock_draw_circle = mock_circle
    surface.mock_transform_smoothscale = (
        mock_smoothscale
    )

    def reset_draw_mocks():
        mock_rect.reset_mock()
        mock_circle.reset_mock()
        mock_smoothscale.reset_mock()
        surface.blit.reset_mock()
        surface.fill.reset_mock()

    surface.reset_draw_mocks = reset_draw_mocks

    return surface


# --- Mock Map Storage ---
@pytest.fixture
def mock_map_storage():
    """Provides map data using codes from mock_static_loader (function scope)."""
    width, height = 20, 15
    return [["black_code"] * width for _ in range(height)]


@pytest.fixture
def mock_pygame_and_exit(mocker):
    """Mocks essential pygame functions and exit."""
    mocker.patch("pygame.error", Exception)

    # Mock image loading
    mock_surface = MagicMock(spec=pygame.Surface)
    mock_surface.convert_alpha.return_value = (
        mock_surface
    )
    mocker.patch("pygame.image.load", return_value=mock_surface)

    # Mock display functions
    mocker.patch("pygame.display.set_icon")
    mocker.patch("pygame.init")
    mocker.patch("pygame.display.init")
    mocker.patch("pygame.display.set_mode")
    mocker.patch("pygame.quit")

    # Mock font loading
    mock_font = MagicMock(spec=pygame.font.Font)
    mocker.patch("pygame.font.Font", return_value=mock_font)
    mocker.patch("pygame.font.init")

    # Mock logging
    mock_logger = MagicMock(spec=logging.Logger)
    mocker.patch("logging.getLogger", return_value=mock_logger)
    mocker.patch("logging.basicConfig")

    # Mock exit
    mocker.patch("builtins.exit")

    return {
        "mock_surface": mock_surface,
        "mock_font": mock_font,
        "mock_logger": mock_logger,
        "mock_exit": mocker.patch("builtins.exit")
    }
