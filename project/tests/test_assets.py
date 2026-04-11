import pytest
import pygame
import logging
from unittest.mock import MagicMock, patch, ANY
import os
from ai_life_simulator._front import assets
from ai_life_simulator._front.assets import StaticLoader


class TestStaticLoader:
    """Тестирует класс StaticLoader, отвечающий за загрузку статических ресурсов."""

    def test_init_success(self, mock_pygame_and_exit):
        """Тестирует успешную инициализацию StaticLoader.

        Проверяет, что логгер настроен, иконка загружена и установлена,
        и программа не завершается с ошибкой.
        """
        loader = StaticLoader()
        logging.basicConfig.assert_called_once()
        logging.getLogger.assert_called_with(assets.__name__)
        pygame.image.load.assert_called_once_with(assets.ICON_PATH)
        pygame.display.set_icon.assert_called_once_with(
            mock_pygame_and_exit["mock_surface"]
        )
        mock_pygame_and_exit["mock_exit"].assert_not_called()
        mock_pygame_and_exit["mock_logger"].info.assert_any_call("Loading Icon...")
        mock_pygame_and_exit["mock_logger"].info.assert_any_call(
            "Icon was successfully loaded"
        )

    def test_init_load_error(self, mock_pygame_and_exit, mocker):
        """Тестирует инициализацию StaticLoader при ошибке загрузки иконки.

        Проверяет, что при ошибке загрузки иконки регистрируется ошибка
        и программа завершается с кодом 1.
        """
        mocker.patch("pygame.image.load", side_effect=pygame.error("Test load error"))
        loader = StaticLoader()
        logging.basicConfig.assert_called_once()
        logging.getLogger.assert_called_with(assets.__name__)
        pygame.image.load.assert_called_once_with(assets.ICON_PATH)
        pygame.display.set_icon.assert_not_called()  # Иконка не должна быть установлена
        mock_pygame_and_exit["mock_logger"].error.assert_called_once_with(
            "Error loading icon", exc_info=True
        )
        mock_pygame_and_exit["mock_exit"].assert_called_once_with(1)

    def test_get_color_methods(self, mock_pygame_and_exit):
        """Тестирует методы получения цветовых схем.

        Проверяет, что все методы, возвращающие цветовые схемы,
        возвращают словари, и что getTrainingPanelColors вызывает getMenuButtonColors.
        """
        loader = StaticLoader()
        assert isinstance(loader.getMenuButtonColors(), dict)
        assert isinstance(loader.getCloseButtonColors(), dict)
        assert isinstance(loader.getToggleButtonColors(), dict)
        assert isinstance(loader.getSwitchColors(), dict)
        assert isinstance(loader.getTrainingPanelColors(), dict)
        # Проверка, что getTrainingPanelColors использует getMenuButtonColors
        with patch.object(
            loader, "getMenuButtonColors", return_value={}
        ) as mock_get_menu:
            loader.getTrainingPanelColors()
            mock_get_menu.assert_called_once()
        assert isinstance(loader.getPaletteColors(), dict)

    def test_get_palette_codes_static(self):
        """Тестирует статический метод getPaletteCodes.

        Проверяет, что метод возвращает словарь и содержит ожидаемые коды палитры.
        """
        codes = StaticLoader.getPaletteCodes()
        assert isinstance(codes, dict)
        assert assets.BROWN in codes
        assert codes[assets.BROWN] == [1, "peaceful"]
        assert 0 in codes
        assert codes[0] == assets.FIELD_COLOR

    def test_get_icon_methods_success(self, mock_pygame_and_exit):
        """Тестирует успешную загрузку иконок через соответствующие методы.

        Проверяет, что иконки перетаскивания и кисти загружаются корректно.
        """
        loader = StaticLoader()
        mock_surface = mock_pygame_and_exit["mock_surface"]
        mock_load = pygame.image.load

        # Сбрасываем моки перед тестом getDragIcon
        mock_load.reset_mock()
        mock_surface.convert_alpha.reset_mock()
        drag_icon = loader.getDragIcon()
        mock_load.assert_called_once_with(assets.DRAG_ICON_PATH)
        mock_surface.convert_alpha.assert_called_once()
        assert drag_icon is mock_surface
        mock_pygame_and_exit["mock_exit"].assert_not_called()

        # Сбрасываем моки перед тестом getBrushIcon
        mock_load.reset_mock()
        mock_surface.convert_alpha.reset_mock()
        brush_icon = loader.getBrushIcon()
        mock_load.assert_called_once_with(assets.BRUSH_ICON_PATH)
        mock_surface.convert_alpha.assert_called_once()
        assert brush_icon is mock_surface
        mock_pygame_and_exit["mock_exit"].assert_not_called()

    def test_get_icon_methods_error(self, mock_pygame_and_exit, mocker):
        """Тестирует обработку ошибок при загрузке иконок.

        Проверяет, что при ошибке загрузки иконок регистрируется ошибка
        и программа завершается с кодом 1.
        """
        mock_load = mocker.patch(
            "pygame.image.load", side_effect=pygame.error("Test icon load error")
        )
        mock_exit = mock_pygame_and_exit["mock_exit"]
        mock_logger = mock_pygame_and_exit["mock_logger"]
        loader = StaticLoader()

        # Тест для getDragIcon при ошибке
        mock_logger.error.reset_mock()
        mock_exit.reset_mock()
        loader.getDragIcon()
        mock_load.assert_called_with(
            assets.DRAG_ICON_PATH
        )  # Проверяем, что попытка загрузки была
        mock_logger.error.assert_called_once_with("Error loading icon", exc_info=True)
        mock_exit.assert_called_once_with(1)

        # Тест для getBrushIcon при ошибке
        mock_logger.error.reset_mock()
        mock_exit.reset_mock()
        loader.getBrushIcon()
        mock_load.assert_called_with(
            assets.BRUSH_ICON_PATH
        )  # Проверяем, что попытка загрузки была
        mock_logger.error.assert_called_once_with("Error loading icon", exc_info=True)
        mock_exit.assert_called_once_with(1)

    def test_get_logger_static(self, mock_pygame_and_exit):
        """Тестирует статический метод getLogger.

        Проверяет, что метод возвращает корректный логгер.
        """
        logger_name = "MySpecificLogger"
        logger = StaticLoader.getLogger(logger_name)
        logging.getLogger.assert_called_with(logger_name)
        assert logger is mock_pygame_and_exit["mock_logger"]

    def test_get_pixel_font_success(self, mock_pygame_and_exit):
        """Тестирует успешную загрузку пиксельного шрифта.

        Проверяет загрузку шрифта с размером по умолчанию и с указанным размером.
        """
        loader = StaticLoader()
        mock_font_loader = pygame.font.Font
        mock_exit = mock_pygame_and_exit["mock_exit"]
        mock_target_font = mock_pygame_and_exit["mock_font"]

        # Тест загрузки шрифта с размером по умолчанию (20)
        mock_font_loader.reset_mock()
        font20 = loader.getPixelFont()
        mock_font_loader.assert_called_once_with(assets.FONT_PATH_PIXEL, 20)
        assert font20 is mock_target_font
        mock_exit.assert_not_called()

        # Тест загрузки шрифта с указанным размером (30)
        mock_font_loader.reset_mock()
        font30 = loader.getPixelFont(font_size=30)
        mock_font_loader.assert_called_once_with(assets.FONT_PATH_PIXEL, 30)
        assert font30 is mock_target_font
        mock_exit.assert_not_called()

    def test_get_pixel_font_error(self, mock_pygame_and_exit, mocker):
        """Тестирует обработку ошибок при загрузке пиксельного шрифта.

        Проверяет, что при ошибке загрузки шрифта регистрируется ошибка
        и программа завершается с кодом 1.
        """
        mock_font_loader = mocker.patch(
            "pygame.font.Font", side_effect=pygame.error("Test font load error")
        )
        mock_exit = mock_pygame_and_exit["mock_exit"]
        mock_logger = mock_pygame_and_exit["mock_logger"]
        loader = StaticLoader()

        mock_logger.error.reset_mock()
        mock_exit.reset_mock()
        loader.getPixelFont()
        mock_font_loader.assert_called_with(
            assets.FONT_PATH_PIXEL, 20
        )  # Проверяем попытку загрузки
        mock_logger.error.assert_called_once_with("Error loading font", exc_info=True)
        mock_exit.assert_called_once_with(1)


def test_constants_paths_exist():
    """Тестирует наличие и тип констант путей в модуле assets.

    Проверяет, что все ожидаемые константы путей являются строками.
    """
    assert isinstance(assets.FONT_PATH_PIXEL, str)
    assert isinstance(assets.ICON_PATH, str)
    assert isinstance(assets.DRAG_ICON_PATH, str)
    assert isinstance(assets.BRUSH_ICON_PATH, str)
    assert isinstance(assets.LOGS_PATH, str)
    assert isinstance(assets.MODEL_PATH, str)
    assert isinstance(assets.PEACEFUL_MODEL_PATH, str)
    assert isinstance(assets.PREDATOR_MODEL_PATH, str)


def test_constants_colors_exist():
    """Тестирует наличие, тип и формат цветовых констант в модуле assets.

    Проверяет, что все ожидаемые цветовые константы являются кортежами
    из трех целых чисел (RGB).
    """
    colors = [
        assets.WHITE,
        assets.FIELD_COLOR,
        assets.BLACK,
        assets.RED,
        assets.GREEN,
        assets.BROWN,
        assets.BLUE,
        assets.GRAY,
        assets.MEDIUM_GRAY,
        assets.DARK_GRAY,
        assets.LIGHT_GRAY,
        assets.SLIDER_TRACK_COLOR,
        assets.SLIDER_HANDLE_COLOR,
        assets.DARK_GREEN,
        assets.LIGHT_BLUE,
        assets.LIGHT_RED,
        assets.YELLOW,
        assets.PURPLE,
        assets.CYAN,
        assets.ORANGE,
    ]
    for color in colors:
        assert isinstance(color, tuple)
        assert len(color) == 3
        assert all(isinstance(c, int) for c in color)


def test_constants_other_exist():
    """Тестирует наличие и тип прочих констант в модуле assets.

    Проверяет тип констант, таких как уровень логирования и доступность CUDA.
    """
    assert isinstance(assets.LOGGING_LEVEL, int)
    assert isinstance(assets.CUDA_IS_AVAILABLE, bool)
