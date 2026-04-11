import logging
import os
import pygame
import torch


FONT_PATH_PIXEL = os.path.join("assets", "PressStart2P-Regular.ttf")
ICON_PATH = "assets/fimos.png"
DRAG_ICON_PATH = "assets/crossroad.png"
BRUSH_ICON_PATH = "assets/paint-brush.png"
LOGS_PATH = "temp/logs.log"
MODEL_PATH = "./saved_models"
PEACEFUL_MODEL_PATH = "peaceful_model_final.pth"
PREDATOR_MODEL_PATH = "predator_model_final.pth"
LOGGING_LEVEL = logging.INFO
CUDA_IS_AVAILABLE = torch.cuda.is_available()


# Цвета
WHITE = (255, 255, 255)
FIELD_COLOR = (189, 237, 57)  # Зеленый цвет игрового поля

BLACK = (0, 0, 0)
RED = (255, 0, 0)
GREEN = (0, 255, 0)
BROWN = (139, 69, 19)  # Коричневый
BLUE = (0, 0, 255)
GRAY = (128, 128, 128)
MEDIUM_GRAY = (80, 80, 80)
DARK_GRAY = (50, 50, 50)
LIGHT_GRAY = (200, 200, 200)
SLIDER_TRACK_COLOR = (100, 100, 100)
SLIDER_HANDLE_COLOR = (210, 210, 210)
BG_COLOR2 = (241, 241, 241)

LIGHT_GRAY = (180, 180, 180)
DARK_GREEN = (0, 150, 0)
LIGHT_BLUE = (50, 100, 200)
LIGHT_RED = (200, 50, 50)
YELLOW = (255, 255, 0)
PURPLE = (128, 0, 128)
CYAN = (0, 150, 150)
ORANGE = (255, 140, 0)


class StaticLoader:
    """
    Класс для загрузки статических ресурсов игры при инициализации.

    Загружает иконку окна при создании экземпляра и предоставляет методы
    для получения шрифтов и цветовых схем для элементов интерфейса.

    Attributes:
        logger (logging.Logger): Экземпляр логгера, используемый классом.

    Warnings:
        - Экземпляр этого класса должен создаваться ПОСЛЕ вызова `pygame.init()`
          и `pygame.display.set_mode()`, так как конструктор (`__init__`)
          вызывает `pygame.display.set_icon()`.
        - Некоторые методы могут вызвать `exit(1)` при ошибках загрузки ресурсов.
    """

    def __init__(self):
        """
        Инициализирует загрузчик и загружает иконку окна.

        Side Effects:
            - Устанавливает иконку окна Pygame.
            - Может завершить программу при ошибке загрузки.
        """
        self.logger = logging.getLogger(__name__)
        logging.basicConfig(
            filename=LOGS_PATH,
            level=LOGGING_LEVEL,
            filemode="a",
            format="%(asctime)s %(levelname)s %(name)s %(message)s",
            encoding="utf-8",
        )
        try:
            self.logger.info("Loading Icon...")
            icon = pygame.image.load(ICON_PATH)
            pygame.display.set_icon(icon)
            self.logger.info("Icon was successfully loaded")
        except pygame.error as e:
            self.logger.error("Error loading icon", exc_info=True)
            exit(1)

    def getMenuButtonColors(self):
        """
        Возвращает цветовую схему для кнопок меню.

        Returns:
            dict: Словарь с цветами для состояний:
                - normal: (tuple) Цвет в обычном состоянии
                - hover: (tuple) Цвет при наведении
                - active: (tuple) Цвет при нажатии
        """
        return {"normal": LIGHT_BLUE, "hover": (70, 120, 240), "active": (40, 80, 180)}

    def getCloseButtonColors(self):
        """
        Возвращает цветовую схему для кнопки закрытия.

        Returns:
            dict: Словарь с цветами для состояний:
                - normal: (tuple) Цвет в обычном состоянии
                - hover: (tuple) Цвет при наведении
                - active: (tuple) Цвет при нажатии
        """
        return {"normal": BLACK, "hover": LIGHT_GRAY, "active": DARK_GRAY}

    def getToggleButtonColors(self):
        """
        Возвращает цветовую схему для переключаемых кнопок.

        Returns:
            dict: Словарь с цветами для состояний:
                - normal/hover/active: Цвета в выключенном состоянии
                - on_normal/on_hover/on_active: Цвета во включенном состоянии
        """
        return {
            "normal": (150, 150, 150),
            "hover": (170, 170, 170),
            "active": (130, 130, 130),
            "on_normal": DARK_GREEN,
            "on_hover": (50, 200, 50),
            "on_active": (0, 120, 0),
        }

    def getSwitchColors(self):
        """
        Возвращает цветовую схему для переключателей.

        Returns:
            dict: Словарь с цветами:
                - bg_off: (tuple) Фон выключенного состояния
                - bg_on: (tuple) Фон включенного состояния
                - knob: (tuple) Цвет ручки
                - knob_hover: (tuple) Цвет ручки при наведении
        """
        return {
            "bg_off": (100, 100, 100),
            "bg_on": DARK_GREEN,
            "knob": WHITE,
            "knob_hover": (240, 240, 240),
        }

    def getTrainingPanelColors(self):
        """
        Возвращает цветовую схему для панели обучения.

        Returns:
            dict: Словарь с цветами для:
                - button: (dict) Цвета кнопок
                - label: (dict) Цвета текста и фона меток
        """
        return {
            "button": self.getMenuButtonColors(),
            "label": {"text": LIGHT_GRAY, "bg": BG_COLOR2},
        }

    def getPaletteColors(self):
        """
        Возвращает цветовую палитру для элементов симуляции.

        Returns:
            dict: Словарь соответствий имен элементов и их цветов в RGB.
        """
        return {
            "peaceful": BROWN,
            "predator": LIGHT_RED,
            "terrain": FIELD_COLOR,
            "food": PURPLE,
            "lake": LIGHT_BLUE,
            "obstacle": DARK_GRAY,
        }

    @staticmethod
    def getPaletteCodes():
        """
        Возвращает коды элементов палитры.

        Returns:
            dict: Словарь соответствий цветов и их кодов с именами.
        """
        return {
            BROWN: [1, "peaceful"],
            LIGHT_RED: [2, "predator"],
            FIELD_COLOR: [0, "terrain"],
            PURPLE: [3, "food"],
            LIGHT_BLUE: [4, "lake"],
            DARK_GRAY: [5, "obstacle"],
            0: FIELD_COLOR,
            1: BROWN,
            2: LIGHT_RED,
            3: PURPLE,
            4: LIGHT_BLUE,
            5: DARK_GRAY,
        }

    def getDragIcon(self):
        """
        Загружает иконку для перетаскивания.

        Returns:
            pygame.Surface: Загруженное изображение иконки.

        Raises:
            SystemExit: При ошибке загрузки изображения.
        """
        try:
            self.logger.info("Loading Drag Icon...")
            return pygame.image.load(DRAG_ICON_PATH).convert_alpha()
        except pygame.error as e:
            self.logger.error("Error loading icon", exc_info=True)
            exit(1)

    def getBrushIcon(self):
        """
        Загружает иконку кисти.

        Returns:
            pygame.Surface: Загруженное изображение иконки.

        Raises:
            SystemExit: При ошибке загрузки изображения.
        """
        try:
            self.logger.info("Loading Drag Icon...")
            return pygame.image.load(BRUSH_ICON_PATH).convert_alpha()
        except pygame.error as e:
            self.logger.error("Error loading icon", exc_info=True)
            exit(1)

    @staticmethod
    def getLogger(logger_name):
        """
        Возвращает логгер с указанным именем.

        Args:
            logger_name (str): Имя логгера.

        Returns:
            logging.Logger: Экземпляр логгера.
        """
        return logging.getLogger(logger_name)

    def getPixelFont(self, font_size=20):
        """
        Загружает пиксельный шрифт указанного размера.

        Args:
            font_size (int, optional): Размер шрифта. По умолчанию 20.

        Returns:
            pygame.font.Font: Загруженный шрифт.

        Raises:
            SystemExit: При ошибке загрузки шрифта.
        """
        try:
            self.logger.info("Loading Font...")
            return pygame.font.Font(FONT_PATH_PIXEL, font_size)
        except pygame.error as e:
            self.logger.error("Error loading font", exc_info=True)
            exit(1)
