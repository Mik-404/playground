import pytest
import pygame
import sys
from unittest.mock import MagicMock, patch, call

from ai_life_simulator._front.cli.main import main, config

WHITE = (255, 255, 255)

# --- Фикстура mock_dependencies---
@pytest.fixture
def mock_dependencies(mocker):
    """Мокает основные зависимости: pygame, sys, GameEngine, StaticLoader."""
    mock_pygame = mocker.patch('ai_life_simulator._front.cli.main.pygame', autospec=True)
    mock_pygame.event.get.return_value = []
    mock_pygame.display.flip = MagicMock()
    mock_pygame.quit = MagicMock()
    mock_clock_instance = MagicMock()
    mock_clock_instance.tick = MagicMock()
    mock_pygame.time.Clock.return_value = mock_clock_instance
    mock_pygame.QUIT = pygame.QUIT
    mock_pygame.KEYDOWN = pygame.KEYDOWN
    mock_pygame.K_ESCAPE = pygame.K_ESCAPE
    mock_pygame.MOUSEBUTTONDOWN = pygame.MOUSEBUTTONDOWN
    mock_pygame.USEREVENT = pygame.USEREVENT

    mock_sys_exit = mocker.patch('ai_life_simulator._front.cli.main.sys.exit')

    mock_game_engine_class = mocker.patch('ai_life_simulator._front.cli.main.GameEngine')
    mock_game_engine_instance = mock_game_engine_class.return_value
    mock_game_engine_instance.fps = config.get('fps', 30)
    mock_game_engine_instance.window = MagicMock(spec=pygame.Surface)
    mock_game_engine_instance.ui_surface = MagicMock(spec=pygame.Surface)
    mock_game_engine_instance.map_view = MagicMock()
    mock_game_engine_instance.map_view.handle_event = MagicMock()
    mock_game_engine_instance.map_view.draw = MagicMock()
    mock_ui_element1 = MagicMock()
    mock_ui_element1.handle_event = MagicMock(return_value=False)
    mock_ui_element1.draw = MagicMock()
    mock_ui_element2 = MagicMock()
    mock_ui_element2.handle_event = MagicMock(return_value=False)
    mock_ui_element2.draw = MagicMock()
    mock_game_engine_instance.ui_elements = [mock_ui_element1, mock_ui_element2]

    mock_static_loader_class = mocker.patch('ai_life_simulator._front.cli.main.StaticLoader')
    mock_logger = MagicMock()
    mock_static_loader_class.getLogger.return_value = mock_logger

    return {
        "pygame": mock_pygame,
        "sys_exit": mock_sys_exit,
        "GameEngine_class": mock_game_engine_class,
        "game_instance": mock_game_engine_instance,
        "StaticLoader_class": mock_static_loader_class,
        "logger": mock_logger,
        "ui_element1": mock_ui_element1,
        "ui_element2": mock_ui_element2,
        "clock_instance": mock_clock_instance,
    }


def test_initialization(mock_dependencies):
    """Тест: Инициализируется ли GameEngine и Logger при запуске?"""
    mock_dependencies["pygame"].event.get.side_effect = [
        [MagicMock(type=mock_dependencies["pygame"].QUIT)],
    ]

    main()

    mock_dependencies["GameEngine_class"].assert_called_once_with(config=config)
    mock_dependencies["StaticLoader_class"].getLogger.assert_called_once_with('ai_life_simulator._front.cli.main')
    mock_dependencies["logger"].warning.assert_any_call("Main Loop is starting")
    mock_dependencies["pygame"].quit.assert_called_once()
    mock_dependencies["sys_exit"].assert_called_once()

def test_quit_event_terminates_loop(mock_dependencies):
    """Тест: Завершает ли событие pygame.QUIT цикл?"""
    mock_event_quit = MagicMock(type=mock_dependencies["pygame"].QUIT)
    mock_dependencies["pygame"].event.get.return_value = [mock_event_quit]

    main()

    mock_dependencies["pygame"].event.get.assert_called()
    mock_dependencies["pygame"].quit.assert_called_once()
    mock_dependencies["sys_exit"].assert_called_once()
    mock_dependencies["game_instance"].window.fill.assert_called_once()

def test_escape_key_terminates_loop(mock_dependencies):
    """Тест: Завершает ли нажатие Escape цикл?"""
    mock_event_escape = MagicMock(type=mock_dependencies["pygame"].KEYDOWN, key=mock_dependencies["pygame"].K_ESCAPE)
    mock_event_quit = MagicMock(type=mock_dependencies["pygame"].QUIT) # Нужно для остановки после кадра с Escape
    mock_dependencies["pygame"].event.get.side_effect = [[mock_event_escape], [mock_event_quit]]

    main()

    assert mock_dependencies["pygame"].event.get.call_count >= 1
    mock_dependencies["pygame"].quit.assert_called_once()
    mock_dependencies["sys_exit"].assert_called_once()
    mock_dependencies["game_instance"].window.fill.assert_called() # Отрисовка должна была быть


def test_event_handling_ui_handles(mock_dependencies):
    """Тест: UI элемент обрабатывает событие, другие UI не вызываются."""
    mock_event_custom = MagicMock(type=mock_dependencies["pygame"].MOUSEBUTTONDOWN)
    stop_exception = StopIteration
    mock_dependencies["pygame"].event.get.side_effect = [[mock_event_custom], stop_exception]

    mock_dependencies["ui_element2"].handle_event.return_value = True
    mock_dependencies["ui_element1"].handle_event.return_value = False

    with pytest.raises(StopIteration):
        main()

    mock_dependencies["game_instance"].map_view.handle_event.assert_called_once_with(mock_event_custom)
    mock_dependencies["ui_element2"].handle_event.assert_called_once_with(mock_event_custom)
    mock_dependencies["ui_element1"].handle_event.assert_not_called()

    mock_dependencies["pygame"].quit.assert_not_called()
    mock_dependencies["sys_exit"].assert_not_called()


def test_event_handling_ui_does_not_handle(mock_dependencies):
    """Тест: UI элементы не обрабатывают событие, оба вызываются."""
    mock_event_custom = MagicMock(type=mock_dependencies["pygame"].MOUSEBUTTONDOWN)
    stop_exception = StopIteration
    mock_dependencies["pygame"].event.get.side_effect = [[mock_event_custom], stop_exception]

    mock_dependencies["ui_element2"].handle_event.return_value = False
    mock_dependencies["ui_element1"].handle_event.return_value = False

    # Запускаем main, ожидая исключение
    with pytest.raises(StopIteration):
        main()

    mock_dependencies["game_instance"].map_view.handle_event.assert_called_once_with(mock_event_custom)
    mock_dependencies["ui_element2"].handle_event.assert_called_once_with(mock_event_custom)
    mock_dependencies["ui_element1"].handle_event.assert_called_once_with(mock_event_custom)

    mock_dependencies["pygame"].quit.assert_not_called()
    mock_dependencies["sys_exit"].assert_not_called()


def test_drawing_calls(mock_dependencies):
    """Тест: Вызываются ли функции отрисовки в правильном порядке (один раз)?"""
    mock_event_other = MagicMock(type=mock_dependencies["pygame"].USEREVENT)
    stop_exception = StopIteration
    mock_dependencies["pygame"].event.get.side_effect = [[mock_event_other], stop_exception]

    # Запускаем main, ожидая исключение
    with pytest.raises(StopIteration):
        main()

    # Проверяем вызовы отрисовки
    mock_window = mock_dependencies["game_instance"].window
    mock_ui_surface = mock_dependencies["game_instance"].ui_surface
    mock_map_view = mock_dependencies["game_instance"].map_view
    mock_ui1 = mock_dependencies["ui_element1"]
    mock_ui2 = mock_dependencies["ui_element2"]
    mock_pygame_display = mock_dependencies["pygame"].display
    mock_tick = mock_dependencies["clock_instance"].tick

    mock_window.fill.assert_called_once_with(WHITE)
    mock_ui1.draw.assert_called_once_with(mock_ui_surface)
    mock_ui2.draw.assert_called_once_with(mock_ui_surface)
    mock_window.blit.assert_called_once_with(mock_ui_surface, (0, 0))
    mock_map_view.draw.assert_called_once_with(mock_window)
    mock_pygame_display.flip.assert_called_once()
    mock_tick.assert_called_once_with(mock_dependencies["game_instance"].fps)

    # Проверим, что стандартный выход не был вызван
    mock_dependencies["pygame"].quit.assert_not_called()
    mock_dependencies["sys_exit"].assert_not_called()