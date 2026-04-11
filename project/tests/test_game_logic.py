# -*- coding: utf-8 -*-
import ctypes
import numpy as np
import pytest
import sys
import threading
import time
from unittest.mock import MagicMock, patch, call, ANY

# --- Пути к модулям ---
GAME_LOGIC_MODULE = 'ai_life_simulator._front.game_logic'
FRONT_MODULE = 'ai_life_simulator._front.front'
ASSETS_MODULE = 'ai_life_simulator._front.assets'
AGENTS_MODULE = 'ai_life_simulator._backend.agents'
THREADING_MODULE = 'threading'
SYS_MODULE = 'sys'

try:
    from ai_life_simulator._front.game_logic import GameState, GameEngine
    from ai_life_simulator._front.assets import (
        StaticLoader, WHITE, FIELD_COLOR, BG_COLOR2, MEDIUM_GRAY,
        CUDA_IS_AVAILABLE, MODEL_PATH, PEACEFUL_MODEL_PATH, PREDATOR_MODEL_PATH
    )
    from ai_life_simulator._backend.agents import Config, SimulationAPI
    from ai_life_simulator._front.front import MapView, Button, Palette, Switch, TrainingPanel
except ImportError as e:
    pytest.skip(f"Пропуск тестов из-за ошибки импорта: {e}", allow_module_level=True)

# --- Глобальные моки и константы для тестов ---
MOCK_CONFIG = {
    'fps' : 30, 'mode' : 0, 'game_zone_size' : 0.65, 'field_size_in_per' : 0.97,
    'standard_tile_height' : 50, 'standard_tile_width' : 60, 'menu_back_height' : 0.85,
    'TitleMarginTopPer': 0.05, 'CloseButtonSizePer' : 0.4, 'CloseButtonMargin' : 0.2,
    'ControlPanelMarginXPer': 0.03, 'PaletteCols': 3, 'SwitchHeightPer': 0.15,
    'SwitchWidthPer': 0.16, 'UpperPanelHeightPer': 0.5, 'ButtonHeightPer': 0.23,
    'ControlPanelWidthPer': 0.4, 'PaletteItemSizePer': 0.14, 'BottomPanelHeightPer': 0.25,
    'PanelMarginPer': 0.06, 'ButtonMarginYPer': 0.03, 'PaletteMarginPer': 0.02,
    'ComputeMargin' : 0.12, 'TraningNegativeMargin' : 0.12
}

MOCK_PALETTE_CODES = {
    "peaceful": (0, "peaceful_color"), "predator": (1, "predator_color"),
    "terrain": (2, "terrain_color"), "food": (3, "food_color"),
    "lake": (4, "lake_color"), "obstacle": (5, "obstacle_color"),
}

MOCK_PALETTE_COLORS = {
    "peaceful": (139, 69, 19), "predator": (200, 50, 50), "terrain": (189, 237, 57),
    "food": (128, 0, 128), "lake": (50, 100, 200), "obstacle": (50, 50, 50),
}

# --- Фикстуры Pytest ---

@pytest.fixture(autouse=True)
def mock_dependencies(mocker):
    """Замокает все внешние зависимости."""

    # --- Мок Pygame ---
    mock_pygame = MagicMock(spec=sys.modules.get('pygame') or object)
    def mock_rect_factory(*args, **kwargs):
        x = args[0] if len(args) > 0 else kwargs.get('left', 0)
        y = args[1] if len(args) > 1 else kwargs.get('top', 0)
        w = args[2] if len(args) > 2 else kwargs.get('width', 0)
        h = args[3] if len(args) > 3 else kwargs.get('height', 0)
        centerx = x + w // 2; centery = y + h // 2
        return MagicMock(x=x, y=y, width=w, height=h, w=w, h=h, left=x, top=y,
                         right=x + w, bottom=y + h, center=(centerx, centery),
                         centerx=centerx, centery=centery, size=(w, h))
    mock_surface_instance = MagicMock()
    mock_surface_instance.get_rect = MagicMock(return_value=mock_rect_factory())
    mock_surface_instance.get_width = MagicMock(return_value=1920)
    mock_surface_instance.get_height = MagicMock(return_value=1080)
    mock_surface_instance.blit = MagicMock()
    mock_surface_instance.fill = MagicMock()
    mock_pygame.init = MagicMock(); mock_pygame.quit = MagicMock()
    mock_pygame.display = MagicMock(); mock_pygame.display.set_caption = MagicMock()
    mock_pygame.display.set_mode = MagicMock(return_value=mock_surface_instance)
    mock_pygame.Surface = MagicMock(return_value=mock_surface_instance)
    mock_pygame.Rect = MagicMock(side_effect=mock_rect_factory)
    mock_pygame.draw = MagicMock(); mock_pygame.draw.rect = MagicMock()
    mock_pygame.event = MagicMock(); mock_pygame.event.post = MagicMock()
    def mock_event_factory(event_type, *args, **kwargs):
        event_mock = MagicMock(); event_mock.type = event_type
        return event_mock
    mock_pygame.event.Event = MagicMock(side_effect=mock_event_factory)
    mock_pygame.font = MagicMock(); mock_pygame.time = MagicMock()
    mock_pygame.QUIT = 12345; mock_pygame.SRCALPHA = 67890
    mock_pygame.FULLSCREEN = 0; mock_pygame.USEREVENT = 99999
    mock_font_render_surface = MagicMock()
    mock_font_render_surface.get_rect = MagicMock(return_value=mock_rect_factory())
    mock_font = MagicMock(); mock_font.render = MagicMock(return_value=mock_font_render_surface)
    mock_pygame.font.Font = MagicMock(return_value=mock_font)
    mock_pygame.font.init = MagicMock(); mock_pygame.font.get_init = MagicMock(return_value=True)
    mock_pygame.time.Clock = MagicMock(return_value=MagicMock(tick=MagicMock()))
    mock_pygame.time.get_ticks = MagicMock(return_value=0); mock_pygame.time.set_timer = MagicMock()
    mocker.patch(f'{FRONT_MODULE}.pygame', mock_pygame)
    mocker.patch(f'{GAME_LOGIC_MODULE}.pygame', mock_pygame, create=True)

    # --- Мок StaticLoader (без логгера) ---
    LOADER_IMPORT_PATH_FOR_GAME_LOGIC = f'{GAME_LOGIC_MODULE}.StaticLoader'
    mock_loader_instance = MagicMock(spec=StaticLoader)
    mock_loader_instance.getPaletteCodes = MagicMock(return_value=MOCK_PALETTE_CODES)
    mock_loader_instance.getPaletteColors = MagicMock(return_value=MOCK_PALETTE_COLORS)
    mock_loader_instance.getPixelFont = MagicMock(return_value=mock_font)
    mock_loader_instance.getCloseButtonColors = MagicMock(return_value=({}, {}, {}))
    mock_loader_instance.getMenuButtonColors = MagicMock(return_value=({}, {}, {}))
    mock_loader_instance.getSwitchColors = MagicMock(return_value={})
    mock_loader_instance.getTrainingPanelColors = MagicMock(return_value={})
    mocker.patch(LOADER_IMPORT_PATH_FOR_GAME_LOGIC, return_value=mock_loader_instance, create=True)

    # --- Мок UI элементов и MapView ---
    # Создаем инстансы моков
    mock_map_view_instance = MagicMock(spec=MapView)
    mock_map_view_instance.rect = mock_rect_factory(); mock_map_view_instance.draw = MagicMock()
    mock_map_view_instance.handle_event = MagicMock(); mock_map_view_instance.set_current_color = MagicMock()
    mock_map_view_instance.ban_brush_mode = MagicMock(); mock_map_view_instance.unlock_brush_mode = MagicMock()

    map_view_class_mock = mocker.patch(f'{GAME_LOGIC_MODULE}.MapView', return_value=mock_map_view_instance, create=True)

    mock_button_instance = MagicMock(spec=Button)
    mock_button_instance.rect = mock_rect_factory(); mock_button_instance.draw = MagicMock()
    mock_button_instance.handle_event = MagicMock()
    button_class_mock = mocker.patch(f'{GAME_LOGIC_MODULE}.Button', return_value=mock_button_instance, create=True)

    mock_palette_instance = MagicMock(spec=Palette)
    mock_palette_instance.rect = mock_rect_factory(); mock_palette_instance.draw = MagicMock()
    mock_palette_instance.handle_event = MagicMock()
    palette_class_mock = mocker.patch(f'{GAME_LOGIC_MODULE}.Palette', return_value=mock_palette_instance, create=True)

    mock_switch_instance = MagicMock(spec=Switch)
    mock_switch_instance.rect = mock_rect_factory(); mock_switch_instance.draw = MagicMock()
    mock_switch_instance.handle_event = MagicMock()
    switch_class_mock = mocker.patch(f'{GAME_LOGIC_MODULE}.Switch', return_value=mock_switch_instance, create=True)

    mock_training_panel_instance = MagicMock(spec=TrainingPanel)
    mock_training_panel_instance.rect = mock_rect_factory(); mock_training_panel_instance.draw = MagicMock()
    mock_training_panel_instance.handle_event = MagicMock(); mock_training_panel_instance.set_status = MagicMock()
    training_panel_class_mock = mocker.patch(f'{GAME_LOGIC_MODULE}.TrainingPanel', return_value=mock_training_panel_instance, create=True)

    # --- Мок Backend классов ---
    mock_config_instance = MagicMock(spec=Config)
    config_class_mock = mocker.patch(f'{GAME_LOGIC_MODULE}.Config', return_value=mock_config_instance, create=True, spec=Config)

    mock_sim_api_instance = MagicMock(spec=SimulationAPI)
    mock_sim_api_instance.start = MagicMock(); mock_sim_api_instance.stop = MagicMock(return_value={"status": "stopped"})
    mock_sim_api_instance.get_map_snapshot = MagicMock(return_value=np.zeros((MOCK_CONFIG["standard_tile_height"], MOCK_CONFIG["standard_tile_width"])))
    mock_sim_api_instance.get_epoch = MagicMock(return_value=0)
    sim_api_class_mock = mocker.patch(f'{GAME_LOGIC_MODULE}.SimulationAPI', return_value=mock_sim_api_instance, create=True, spec=SimulationAPI)

    # --- Мок threading ---
    mock_thread_instance = MagicMock(spec=threading.Thread)
    mock_thread_instance.start = MagicMock()
    thread_class_mock = mocker.patch(f'{GAME_LOGIC_MODULE}.threading.Thread', return_value=mock_thread_instance, create=True, spec=threading.Thread)

    # --- Мок time.sleep ---
    mocker.patch(f'{GAME_LOGIC_MODULE}.time.sleep', MagicMock(), create=True)

    # --- Мок ctypes ---
    mock_ctypes = MagicMock(); mock_ctypes.windll = MagicMock()
    mock_ctypes.windll.shcore = MagicMock(); mock_ctypes.windll.shcore.SetProcessDpiAwareness = MagicMock()
    mocker.patch(f'{GAME_LOGIC_MODULE}.ctypes', mock_ctypes, create=True)

    # --- Мок констант и переменных окружения ---
    mocker.patch(f'{ASSETS_MODULE}.CUDA_IS_AVAILABLE', False)
    mocker.patch(f'{GAME_LOGIC_MODULE}.CUDA_IS_AVAILABLE', False, create=True)
    mocker.patch(f'{ASSETS_MODULE}.MODEL_PATH', "mock/model/path")
    mocker.patch(f'{GAME_LOGIC_MODULE}.MODEL_PATH', "mock/model/path", create=True)
    mocker.patch(f'{ASSETS_MODULE}.PEACEFUL_MODEL_PATH', "peaceful.pth")
    mocker.patch(f'{GAME_LOGIC_MODULE}.PEACEFUL_MODEL_PATH', "peaceful.pth", create=True)
    mocker.patch(f'{ASSETS_MODULE}.PREDATOR_MODEL_PATH', "predator.pth")
    mocker.patch(f'{GAME_LOGIC_MODULE}.PREDATOR_MODEL_PATH', "predator.pth", create=True)
    mocker.patch(f'{GAME_LOGIC_MODULE}.FIELD_COLOR', FIELD_COLOR, create=True)
    mocker.patch(f'{GAME_LOGIC_MODULE}.MEDIUM_GRAY', MEDIUM_GRAY, create=True)
    mocker.patch(f'{GAME_LOGIC_MODULE}.WHITE', WHITE, create=True)
    mocker.patch(f'{GAME_LOGIC_MODULE}.BG_COLOR2', BG_COLOR2, create=True)

    # --- Мок sys ---
    mock_sys = MagicMock(spec=sys); mock_sys.platform = 'linux'
    mocker.patch(f'{GAME_LOGIC_MODULE}.sys', mock_sys, create=True)

    return {
        "pygame": mock_pygame,
        "loader_instance": mock_loader_instance,

        "Config_class_mock": config_class_mock,
        "SimulationAPI_class_mock": sim_api_class_mock,
        "Thread_class_mock": thread_class_mock,
        "MapView_class_mock": map_view_class_mock,
        "Button_class_mock": button_class_mock,
        "Palette_class_mock": palette_class_mock,
        "Switch_class_mock": switch_class_mock,
        "TrainingPanel_class_mock": training_panel_class_mock,

        "config_instance": mock_config_instance,
        "sim_api_instance": mock_sim_api_instance,
        "thread_instance": mock_thread_instance,
        "map_view_instance": mock_map_view_instance,
        "button_instance": mock_button_instance,
        "palette_instance": mock_palette_instance,
        "switch_instance": mock_switch_instance,
        "training_panel_instance": mock_training_panel_instance,

        "ctypes": mock_ctypes, "sys": mock_sys,
        "CUDA_IS_AVAILABLE_PATH": f'{GAME_LOGIC_MODULE}.CUDA_IS_AVAILABLE',
    }

@pytest.fixture
def game_state(mock_dependencies):
    """Фикстура для создания экземпляра GameState."""
    state = GameState()
    def mock_get_code_for_type(state_type):
        code_tuple = MOCK_PALETTE_CODES.get(state_type)
        return code_tuple[0] if code_tuple else 0
    try: state._get_code_for_type = mock_get_code_for_type
    except AttributeError:
        if hasattr(state, 'palette_codes'): state.palette_codes = MOCK_PALETTE_CODES
    return state

@pytest.fixture
def game_engine(mocker, game_state, mock_dependencies):
    """Фикстура для создания экземпляра GameEngine с замоканными зависимостями."""
    mocker.patch(f'{GAME_LOGIC_MODULE}.GameState', return_value=game_state)
    mocker.patch(mock_dependencies["CUDA_IS_AVAILABLE_PATH"], False)

    for key, value in mock_dependencies.items():
        if isinstance(value, MagicMock):
            if hasattr(value, 'reset_mock') and callable(value.reset_mock): value.reset_mock()
    mock_dependencies["Config_class_mock"].reset_mock()
    mock_dependencies["SimulationAPI_class_mock"].reset_mock()
    mock_dependencies["Thread_class_mock"].reset_mock()
    mock_dependencies["MapView_class_mock"].reset_mock()
    mock_dependencies["Button_class_mock"].reset_mock()
    mock_dependencies["Palette_class_mock"].reset_mock()
    mock_dependencies["Switch_class_mock"].reset_mock()
    mock_dependencies["TrainingPanel_class_mock"].reset_mock()

    engine = GameEngine(MOCK_CONFIG)

    engine.game_state = game_state
    engine.map_view = mock_dependencies["map_view_instance"]
    engine.palette = mock_dependencies["palette_instance"]
    engine.compute_switch = mock_dependencies["switch_instance"]
    engine.training_panel = mock_dependencies["training_panel_instance"]
    engine.agents_state = mock_dependencies["config_instance"]

    return engine

# --- Тесты для GameState ---
class TestGameState:
    def test_init(self, game_state, mock_dependencies):
        """Тестирует инициализацию GameState."""
        assert game_state.peaceful_units == []
        assert game_state.predators == []
        assert isinstance(game_state.map_data, np.ndarray)
        assert game_state.map_data.shape == (MOCK_CONFIG["standard_tile_height"], MOCK_CONFIG["standard_tile_width"])
        assert np.all(game_state.map_data == 0)
        assert game_state.current_color == FIELD_COLOR
        assert not game_state.is_running
        assert not game_state.map_maybe_updated

    def test_setup(self, game_state, mock_dependencies):
        """Тестирует сброс состояния GameState."""
        game_state.peaceful_units = [1]
        game_state.predators = [2]
        if game_state.map_data.size > 0: game_state.map_data[0, 0] = 99
        game_state.current_color = (1, 1, 1); game_state.is_running = True
        game_state.map_maybe_updated = True; game_state.compute_mode = "GPU"
        game_state.setup()
        assert game_state.peaceful_units == []
        assert game_state.predators == []
        assert isinstance(game_state.map_data, np.ndarray)
        assert game_state.map_data.shape == (MOCK_CONFIG["standard_tile_height"], MOCK_CONFIG["standard_tile_width"])
        assert np.all(game_state.map_data == 0)
        assert game_state.current_color == FIELD_COLOR
        assert not game_state.is_running
        assert not game_state.map_maybe_updated

    def test_set_map_data(self, game_state, mock_dependencies):
        """Тестирует обновление данных карты."""
        row, col = 10, 20; state_type = "predator"
        expected_code = MOCK_PALETTE_CODES[state_type][0]
        game_state.set_map_data(row, col, state_type)
        actual_value = game_state.map_data[row, col]
        assert actual_value == expected_code

# --- Тесты для GameEngine ---

class TestGameEngine:

    def test_init_calls(self, game_engine, mock_dependencies, mocker):
        """Тестирует основные вызовы моков во время инициализации GameEngine."""
        pygame_mock = mock_dependencies["pygame"]
        config_class_mock = mock_dependencies["Config_class_mock"]
        map_view_class_mock = mock_dependencies["MapView_class_mock"]
        button_class_mock = mock_dependencies["Button_class_mock"]
        palette_class_mock = mock_dependencies["Palette_class_mock"]
        switch_class_mock = mock_dependencies["Switch_class_mock"]
        training_panel_class_mock = mock_dependencies["TrainingPanel_class_mock"]

        pygame_mock.init.assert_called_once()
        pygame_mock.display.set_caption.assert_called_once_with("fimos", icontitle="fimos")
        pygame_mock.display.set_mode.assert_called_once_with((0, 0), pygame_mock.FULLSCREEN, vsync=1)

        config_class_mock.assert_called_once_with(
            MAP_WIDTH=MOCK_CONFIG["standard_tile_width"], MAP_HEIGHT=MOCK_CONFIG["standard_tile_height"],
            SAVE_MODEL_DIR=ANY, PEACEFUL_MODEL_FILENAME=ANY, PREDATOR_MODEL_FILENAME=ANY, DEVICE="cpu"
        )
        assert game_engine.agents_state is mock_dependencies["config_instance"]

        assert game_engine.ui_surface is not None

        assert pygame_mock.Rect.call_count >= 3
        map_view_class_mock.assert_called_once()
        assert game_engine.map_view is mock_dependencies["map_view_instance"]
        assert pygame_mock.draw.rect.call_count >= 3

        button_class_mock.assert_called()
        palette_class_mock.assert_called_once()
        switch_class_mock.assert_called_once()
        training_panel_class_mock.assert_called_once()
        assert len(game_engine.ui_elements) > 0

    def test_setup_dpi_windows(self, mocker, mock_dependencies):
        """Тестирует _setup_dpi на Windows."""
        mock_ctypes = mock_dependencies["ctypes"]; mock_sys = mock_dependencies["sys"]
        mock_sys.platform = 'win32'
        with patch(f'{GAME_LOGIC_MODULE}.GameEngine._create_ui_background_elements', MagicMock()), \
             patch(f'{GAME_LOGIC_MODULE}.GameEngine._create_ui_elements', MagicMock()), \
             patch(f'{GAME_LOGIC_MODULE}.GameEngine.setup_nn_agents', MagicMock()):
            engine = GameEngine(MOCK_CONFIG)
        mock_ctypes.windll.shcore.SetProcessDpiAwareness.reset_mock()
        engine._setup_dpi()
        mock_ctypes.windll.shcore.SetProcessDpiAwareness.assert_called_once_with(1)
        mock_sys.platform = 'linux'

    def test_setup_dpi_non_windows_import_error(self, mocker, mock_dependencies):
        """Тестирует _setup_dpi при ImportError ctypes."""
        mocker.patch(f'{GAME_LOGIC_MODULE}.ctypes', side_effect=ImportError("Cannot import ctypes"), create=True)
        mock_sys = mock_dependencies["sys"]; mock_sys.platform = 'linux'
        with patch(f'{GAME_LOGIC_MODULE}.GameEngine._create_ui_background_elements', MagicMock()), \
             patch(f'{GAME_LOGIC_MODULE}.GameEngine._create_ui_elements', MagicMock()), \
             patch(f'{GAME_LOGIC_MODULE}.GameEngine.setup_nn_agents', MagicMock()):
            engine = GameEngine(MOCK_CONFIG)
        engine._setup_dpi()

    def test_setup_dpi_non_windows_attribute_error(self, mocker, mock_dependencies):
        """Тестирует _setup_dpi при AttributeError (нет windll)."""
        mock_ctypes = MagicMock(); del mock_ctypes.windll
        mocker.patch(f'{GAME_LOGIC_MODULE}.ctypes', mock_ctypes, create=True)
        mock_sys = mock_dependencies["sys"]; mock_sys.platform = 'win32'
        with patch(f'{GAME_LOGIC_MODULE}.GameEngine._create_ui_background_elements', MagicMock()), \
             patch(f'{GAME_LOGIC_MODULE}.GameEngine._create_ui_elements', MagicMock()), \
             patch(f'{GAME_LOGIC_MODULE}.GameEngine.setup_nn_agents', MagicMock()):
            engine = GameEngine(MOCK_CONFIG)
        engine._setup_dpi()
        mock_sys.platform = 'linux'

    def test_setup_nn_agents_cpu(self, game_engine, mock_dependencies, mocker):
        """Проверяет настройку агентов НС для CPU."""
        mocker.patch(mock_dependencies["CUDA_IS_AVAILABLE_PATH"], False)
        game_engine.game_state.compute_mode = "CPU"
        config_class_mock = mock_dependencies["Config_class_mock"]
        config_class_mock.reset_mock()
        game_engine.setup_nn_agents()
        config_class_mock.assert_called_once_with(ANY, MAP_HEIGHT=ANY, SAVE_MODEL_DIR=ANY, PEACEFUL_MODEL_FILENAME=ANY, PREDATOR_MODEL_FILENAME=ANY, DEVICE="cpu")
        assert game_engine.agents_state is config_class_mock.return_value

    def test_setup_nn_agents_gpu_available(self, game_engine, mock_dependencies, mocker):
        """Проверяет настройку агентов НС для GPU, когда CUDA доступно."""
        mocker.patch(mock_dependencies["CUDA_IS_AVAILABLE_PATH"], True)
        game_engine.game_state.compute_mode = "GPU"
        config_class_mock = mock_dependencies["Config_class_mock"]
        config_class_mock.reset_mock()
        game_engine.setup_nn_agents()
        config_class_mock.assert_called_once_with(ANY, MAP_HEIGHT=ANY, SAVE_MODEL_DIR=ANY, PEACEFUL_MODEL_FILENAME=ANY, PREDATOR_MODEL_FILENAME=ANY, DEVICE="cuda")
        assert game_engine.agents_state is config_class_mock.return_value

    def test_setup_nn_agents_gpu_not_available(self, game_engine, mock_dependencies, mocker):
        """Проверяет настройку агентов НС для GPU, когда CUDA доступно."""
        mocker.patch(mock_dependencies["CUDA_IS_AVAILABLE_PATH"], False)
        game_engine.game_state.compute_mode = "GPU"
        config_class_mock = mock_dependencies["Config_class_mock"]
        config_class_mock.reset_mock()
        game_engine.setup_nn_agents()
        config_class_mock.assert_called_once_with(ANY, MAP_HEIGHT=ANY, SAVE_MODEL_DIR=ANY, PEACEFUL_MODEL_FILENAME=ANY, PREDATOR_MODEL_FILENAME=ANY, DEVICE="cpu")
        assert game_engine.agents_state is config_class_mock.return_value

    def test_close_game_action_running(self, game_engine, mock_dependencies):
        """Тестирует закрытие игры, когда симуляция запущена."""
        game_engine.game_state.is_running = True
        game_engine.kill_func = MagicMock()
        mock_pygame = mock_dependencies["pygame"]
        mock_pygame.event.post.reset_mock(); mock_pygame.event.Event.reset_mock()

        game_engine.close_game_action()

        game_engine.kill_func.assert_called_once()
        mock_pygame.event.post.assert_called_once()
        mock_pygame.event.Event.assert_called_once_with(mock_pygame.QUIT)
        event_arg = mock_pygame.event.post.call_args[0][0]
        assert event_arg.type == mock_pygame.QUIT

    def test_close_game_action_not_running(self, game_engine, mock_dependencies):
        """Тестирует закрытие игры, когда симуляция не запущена."""
        game_engine.game_state.is_running = False
        game_engine.kill_func = MagicMock()
        mock_pygame = mock_dependencies["pygame"]
        mock_pygame.event.post.reset_mock(); mock_pygame.event.Event.reset_mock()

        game_engine.close_game_action()

        game_engine.kill_func.assert_not_called()
        mock_pygame.event.post.assert_called_once()
        mock_pygame.event.Event.assert_called_once_with(mock_pygame.QUIT)
        event_arg = mock_pygame.event.post.call_args[0][0]
        assert event_arg.type == mock_pygame.QUIT

    def test_get_palette_items_data(self, game_engine, mock_dependencies):
        """Тестирует получение данных для палитры."""
        game_engine.loader = mock_dependencies["loader_instance"]
        game_engine.loader.getPaletteColors.return_value = MOCK_PALETTE_COLORS
        items = game_engine.get_palette_items_data()
        assert len(items) == 6
        expected_types = ["peaceful", "predator", "terrain", "food", "lake", "obstacle"]
        for i, item in enumerate(items):
            assert item["type"] == expected_types[i]
            assert item["color"] == MOCK_PALETTE_COLORS[expected_types[i]]
            assert item["icon"] is None

    def test_kill_func(self, game_engine, mock_dependencies):
        """Тестирует остановку симуляции через kill_func."""
        mock_sim_api = mock_dependencies["sim_api_instance"]
        mock_map_view = mock_dependencies["map_view_instance"]
        game_engine.sim_api = mock_sim_api
        mock_sim_api.get_epoch.return_value = 100
        game_engine.game_state.map_maybe_updated = True
        mock_sim_api.reset_mock(); mock_map_view.reset_mock()

        game_engine.kill_func()

        assert not game_engine.game_state.map_maybe_updated
        mock_sim_api.get_epoch.assert_called_once()
        mock_sim_api.stop.assert_called_once_with(timeout=10)
        mock_map_view.unlock_brush_mode.assert_called_once()

    def test_start_func(self, game_engine, mock_dependencies):
        """Тестирует запуск симуляции через start_func."""
        sim_api_class_mock = mock_dependencies["SimulationAPI_class_mock"]
        sim_api_instance_mock = mock_dependencies["sim_api_instance"]
        thread_class_mock = mock_dependencies["Thread_class_mock"]
        thread_instance_mock = mock_dependencies["thread_instance"]
        map_view_mock = mock_dependencies["map_view_instance"]

        sim_api_class_mock.reset_mock()
        sim_api_instance_mock.reset_mock()
        thread_class_mock.reset_mock()
        thread_instance_mock.reset_mock()
        map_view_mock.reset_mock()

        game_engine.start_func()

        sim_api_class_mock.assert_called_once_with(game_engine.game_state.map_data, game_engine.agents_state)
        game_engine.sim_api.start.assert_called_once()
        map_view_mock.ban_brush_mode.assert_called_once()
        assert game_engine.game_state.map_maybe_updated
        thread_class_mock.assert_called_once_with(target=game_engine.map_loader)

    def test_map_loader_single_iteration(self, game_engine, mock_dependencies, mocker):
        """Тестирует один цикл работы map_loader."""
        map_height, map_width = MOCK_CONFIG["standard_tile_height"], MOCK_CONFIG["standard_tile_width"]
        mock_snapshot = np.arange(map_height * map_width).reshape((map_height, map_width)) % 7
        mock_sim_api = mock_dependencies["sim_api_instance"]
        game_engine.sim_api = mock_sim_api
        game_engine.game_state.map_maybe_updated = True
        call_count = 0
        def side_effect_get_map():
            nonlocal call_count; call_count += 1
            if call_count > 1: game_engine.game_state.map_maybe_updated = False
            return np.copy(mock_snapshot)
        mock_sim_api.get_map_snapshot.side_effect = side_effect_get_map
        mock_sleep = mocker.patch(f'{GAME_LOGIC_MODULE}.time.sleep')
        mock_sim_api.get_map_snapshot.reset_mock()

        game_engine.map_loader()

        mock_sim_api.get_map_snapshot.assert_called()
        np.testing.assert_array_equal(game_engine.game_state.map_data, mock_snapshot)
        mock_sleep.assert_called_with(0.1)
        assert not game_engine.game_state.map_maybe_updated