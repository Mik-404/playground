from ai_life_simulator._front.assets import StaticLoader, WHITE
from ai_life_simulator._front.front import *
from ai_life_simulator._backend.agents import *
import ctypes

class GameState:
    """
    Управляет динамическим состоянием игры, включая юнитов, информацию о карте
    и другие игровые переменные.
    """

    def __init__(self):
        """
        Инициализирует состояние игры значениями по умолчанию.
        
        Args:
            None: Ничего не принимает.
        
        Returns:
            None: Ничего не возвращает.
        """
        self.logger = StaticLoader.getLogger(__name__)
        self.setup()
        self.logger.info("GameState initialized.")

    def setup(self):
        """
        Сбрасывает состояние игры к начальным значениям по умолчанию.
        
        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        self.logger.info("Resetting GameState.")
        self.peaceful_units = []
        self.predators = []

        self.map_data = np.zeros((50, 60), dtype="int")
        self.current_color = FIELD_COLOR
        self.is_running = False
        self.map_maybe_updated = False
        self.compute_mode = -1

    def set_map_data(self, row, col, new_state):
        """
        Устанавливает код типа объекта для указанной ячейки на карте.

        Args:
            row (int): Индекс строки ячейки на карте.
            col (int): Индекс столбца ячейки на карте.
            new_state_key (str): Строковый ключ типа объекта (например, 'peaceful', 'terrain'),
                                 используемый для получения кода из палитры.
        Returns:
            None: Ничего не возвращает.
        """
        self.map_data[row][col] = StaticLoader.getPaletteCodes()[new_state][0]
        self.logger.info("Map data updated.")


class GameEngine:
    """
    Обрабатывает начальную настройку приложения Pygame, управляет основным
    игровым циклом и пользовательским интерфейсом.
    """

    def __init__(self, config):
        """
        Инициализирует Pygame, окно, элементы UI и состояние игры.

        Args:
            config (dict): Словарь конфигурации, содержащий настройки игры (например, fps, размеры).

        Returns:
            None: Ничего не возвращает.
        """
        self.config = config
        self.fps = config["fps"]

        self.logger = StaticLoader.getLogger(__name__)
        self._setup_dpi()

        pygame.init()
        self.loader = StaticLoader()
        self.game_state = GameState()
        self.setup_nn_agents()

        self._setup_display()

        self.winwidth = self.window.get_width()
        self.winheight = self.window.get_height()

        self._create_ui_surface()
        self._load_fonts()

        # Теперь создаем UI
        self._create_ui_background_elements()
        self._create_ui_elements()

        self.logger.warning("Initialization complete. Main Loop is starting.")

    def _setup_dpi(self):
        """
        Настраивает DPI для Windows для корректного масштабирования.

        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        try:
            ctypes.windll.shcore.SetProcessDpiAwareness(1)
            self.logger.info("DPI awareness set (Windows).")
        except (ImportError, AttributeError):
            self.logger.warning(
                "Could not set DPI awareness (not Windows or ctypes/shcore unavailable).",
                exc_info=True,
            )

    def _setup_display(self):
        """
        Инициализирует и настраивает главное окно Pygame.

        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        self.logger.info("Window is being adjusted")
        pygame.display.set_caption("fimos", icontitle="fimos")
        desktop_sizes = pygame.display.get_desktop_sizes()
        # Создаем окно в полноэкранном режиме
        self.window = pygame.display.set_mode((0, 0), self.config["mode"], vsync=1)

    def setup_nn_agents(self):
        """
        Настраивает конфигурацию для агентов нейронной сети.
        
        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        self.agents_state = Config(
            MAP_WIDTH=self.config["standard_tile_width"],
            MAP_HEIGHT=self.config["standard_tile_height"],
            SAVE_MODEL_DIR=MODEL_PATH,
            PEACEFUL_MODEL_FILENAME=PEACEFUL_MODEL_PATH,
            PREDATOR_MODEL_FILENAME=PREDATOR_MODEL_PATH,
            DEVICE="cuda"
            if CUDA_IS_AVAILABLE and self.game_state.compute_mode == "GPU"
            else "cpu",
        )

    def _create_ui_surface(self):
        """
        Создает поверхность Pygame для отрисовки элементов пользовательского интерфейса.
        Поверхность создается с поддержкой альфа-канала для прозрачности.
        
        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        self.logger.info("Creating UI elements...")
        # Создаем поверхность для UI с поддержкой прозрачности
        self.ui_surface = pygame.Surface(
            (self.winwidth, self.winheight), pygame.SRCALPHA
        )

    def _create_ui_background_elements(self):
        """
        Создает и отрисовывает статические фоновые элементы пользовательского интерфейса.
        
        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        self.BGFigure1 = pygame.Rect(
            0, 0, self.winwidth * self.config["game_zone_size"], self.winheight
        )

        # 2. Активное игровое поле (зеленое)
        field_width = int(self.BGFigure1.width * self.config["field_size_in_per"])
        field_height = int(self.BGFigure1.height * self.config["field_size_in_per"])
        self.BGFigure2 = pygame.Rect(0, 0, field_width, field_height)
        self.BGFigure2.center = self.BGFigure1.center  # Центрируем поле в игровой зоне

        self.map_view = MapView(
            viewport_rect=self.BGFigure2,
            map_width_tiles=self.config["standard_tile_width"],
            map_height_tiles=self.config["standard_tile_height"],
            loader=self.loader,
            map_storage=self.game_state.map_data,
        )

        # 3. Правая зона меню (фон)
        menu_zone_width = self.winwidth * (1 - self.config["game_zone_size"])
        self.menu_bg_width = int(menu_zone_width * self.config["field_size_in_per"])
        menu_bg_height = int(self.winheight * self.config["menu_back_height"])
        menu_bg_x = (
            self.winwidth - self.menu_bg_width
        )  # Прижимаем к левому нижнему углу
        self.menu_bg_y = self.winheight - menu_bg_height
        self.BGFigure3 = pygame.Rect(
            menu_bg_x, self.menu_bg_y, self.menu_bg_width, menu_bg_height
        )

        # --- Отрисовка статичных фонов ---
        pygame.draw.rect(self.ui_surface, BG_COLOR2, self.BGFigure1)  # Фон игровой зоны
        pygame.draw.rect(
            self.ui_surface, FIELD_COLOR, self.BGFigure2, border_radius=15
        )  # Игровое поле
        pygame.draw.rect(
            self.ui_surface, BG_COLOR2, self.BGFigure3, border_radius=10
        )  # Фон меню

    def _load_fonts(self):
        """
        Загружает шрифты, используемые в пользовательском интерфейсе.
        Размеры шрифтов рассчитываются относительно размеров окна и конфигурационных параметров.
        
        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        font_size_reference = self.winwidth * (1 - self.config["game_zone_size"]) * self.config["field_size_in_per"]
        self.title_font = self.loader.getPixelFont(int(font_size_reference / 23))
        self.button_font = self.loader.getPixelFont(int(self.config["CloseButtonSizePer"] * font_size_reference / 11))
        self.small_button_font = self.loader.getPixelFont(int(self.config["CloseButtonSizePer"] * font_size_reference / 14))
        self.label_font = self.loader.getPixelFont(int(self.config["CloseButtonSizePer"] * font_size_reference / 18))

    def close_game_action(self):
        """
        Выполняет действия при запросе на закрытие игры (например, нажатие кнопки "Закрыть").
        Если симуляция запущена, останавливает ее.
        
        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        self.logger.warning("Shutdown requested via button.")
        if self.game_state.is_running:
            self.kill_func()
        pygame.event.post(pygame.event.Event(pygame.QUIT))

    def _create_ui_elements(self):
        """
        Создает все интерактивные элементы пользовательского интерфейса.
        Элементы включают заголовок, кнопку закрытия, панель управления (Старт/Пауза, Сброс),
        палитру выбора объектов, переключатель CPU/GPU и панель обучения.
        Все элементы добавляются в список `self.ui_elements` для дальнейшей обработки.

        Args:
            None: Ничего не принимает.
        
        Returns:
            None: Ничего не возвращает.
        """
        self.ui_elements = (
            []
        )  # Список всех активных UI элементов для обработки событий и отрисовки

        bg_rect = self.BGFigure3

        # --- 1. Заголовок ---
        title_text = "Game Controls"
        title_text_surface = self.title_font.render(title_text, True, MEDIUM_GRAY)
        title_rect = title_text_surface.get_rect()
        title_rect.centerx = bg_rect.centerx
        title_rect.top = bg_rect.top + bg_rect.height * self.config["TitleMarginTopPer"]

        self.ui_surface.blit(title_text_surface, title_rect)

        # --- 2. Кнопка Закрыть (вверху справа) ---
        button_height = self.menu_bg_y / 2
        button_width = self.config["CloseButtonSizePer"] * self.menu_bg_width
        self.ui_elements.append(
            Button(
                self.winwidth
                - (button_width + button_width * self.config["CloseButtonMargin"]),
                (self.menu_bg_y - button_height) / 2,
                button_width,
                button_height,
                self.button_font,
                "х Закрыть",
                self.loader.getCloseButtonColors(),
                action=self.close_game_action,
            )
        )

        # --- Определяем области для панелей ---
        # Верхняя панель
        upper_panel_bottom_y = (
            title_rect.bottom + bg_rect.height * self.config["PanelMarginPer"]
        )
        upper_panel_left_x = (
            bg_rect.left + bg_rect.width * self.config["PanelMarginPer"]
        )

        upper_panel_height = (bg_rect.height) * self.config["UpperPanelHeightPer"]
        upper_panel_width = (bg_rect.width) - bg_rect.width * self.config[
            "PanelMarginPer"
        ] * 2

        # Нижняя панель
        bottom_panel_bottom_y = (
            upper_panel_bottom_y
            + upper_panel_height
            + bg_rect.height * self.config["PanelMarginPer"]
        )
        bottom_panel_left_x = (
            bg_rect.left + bg_rect.width * self.config["PanelMarginPer"]
        )

        bottom_panel_height = bg_rect.height * self.config["BottomPanelHeightPer"]
        bottom_panel_width = (bg_rect.width) - bg_rect.width * self.config[
            "PanelMarginPer"
        ] * 2

        # --- 3. Панель управления (Старт/Пауза, Сброс) ---
        btn_width = upper_panel_width * self.config["ControlPanelWidthPer"]
        btn_height = upper_panel_height * self.config["ButtonHeightPer"]
        btn_margin_y = upper_panel_height * self.config["ButtonMarginYPer"]
        btn_x = upper_panel_left_x

        current_y = upper_panel_bottom_y + btn_margin_y

        # Кнопка Старт/Пауза
        self.play_pause_button = Button(
            btn_x,
            current_y,
            btn_width,
            btn_height,
            self.button_font,
            text="Stop",
            colors=self.loader.getMenuButtonColors(),
            action=self.stop_action,
            text_color=WHITE,
        )
        self.ui_elements.append(self.play_pause_button)
        current_y += btn_height + btn_margin_y

        # Кнопка Сброс
        self.reset_button = Button(
            btn_x,
            current_y,
            btn_width,
            btn_height,
            self.button_font,
            text="Reset",
            colors=self.loader.getMenuButtonColors(),
            action=self.reset_game_action,
            text_color=WHITE,
        )
        self.ui_elements.append(self.reset_button)

        # --- 4. Палитра выбора объектов ---
        palette_panel_x = btn_width + btn_x
        palette_panel_width = bg_rect.right - palette_panel_x
        palette_panel_rect = pygame.Rect(
            palette_panel_x,
            btn_margin_y + upper_panel_bottom_y,
            palette_panel_width,
            upper_panel_height,
        )

        item_size = palette_panel_rect.width * self.config["PaletteItemSizePer"]
        margin = palette_panel_rect.width * self.config["PaletteMarginPer"]
        cols = self.config["PaletteCols"]

        # Подгоняем размер палитры под содержимое
        items_width = cols * item_size + (cols - 1) * margin
        rows = (len(self.get_palette_items_data()) + cols - 1) // cols
        items_height = rows * item_size + (rows - 1) * margin

        title_text = "Выберите цвет"
        title_text_surface = self.label_font.render(title_text, True, MEDIUM_GRAY)
        title_rect = title_text_surface.get_rect()
        title_rect.centerx = palette_panel_rect.centerx
        title_rect.top = palette_panel_rect.top

        self.ui_surface.blit(title_text_surface, title_rect)

        # Центрируем область палитры внутри palette_panel_rect
        palette_actual_x_margin = (palette_panel_rect.width - items_width) / 2
        palette_actual_x = palette_actual_x_margin + palette_panel_rect.left
        palette_actual_y = title_rect.bottom + margin * 2

        self.palette = Palette(
            palette_actual_x,
            palette_actual_y,
            items_width,
            items_height,
            items_data=self.get_palette_items_data(),
            cols=cols,
            item_size=item_size,
            margin=margin,
            on_select=self.select_tool_action,
        )
        self.ui_elements.append(self.palette)

        # --- 5. Нижняя панель (Переключатель CPU/GPU и Обучение) ---
        bottom_panel_rect = pygame.Rect(
            bottom_panel_left_x,
            bottom_panel_bottom_y,
            bottom_panel_width,
            bottom_panel_height,
        )

        # Переключатель CPU/GPU (слева внизу)
        switch_width = bottom_panel_rect.width * self.config["SwitchWidthPer"]
        switch_height = bottom_panel_rect.height * self.config["SwitchHeightPer"]
        switch_x = (
            bottom_panel_rect.left
            + bottom_panel_rect.width * self.config["ComputeMargin"]
        )
        switch_y = bottom_panel_rect.centery - (switch_height / 2)

        self.compute_switch = Switch(
            switch_x,
            switch_y,
            switch_width,
            switch_height,
            self.label_font,
            labels=("CPU", "GPU"),
            colors=self.loader.getSwitchColors(),
            action=self.set_compute_mode_action,
            initial_state=(self.game_state.compute_mode == "GPU"),
        )
        self.ui_elements.append(self.compute_switch)

        # Панель обучения (справа внизу)
        train_panel_width = bottom_panel_rect.width * 0.6
        train_panel_height = bottom_panel_rect.height * 0.8
        train_panel_x = bottom_panel_rect.right - train_panel_width
        train_panel_y = (
            self.compute_switch.rect.top
            - bottom_panel_rect.height * self.config["TraningNegativeMargin"]
        )
        self.training_panel = TrainingPanel(
            train_panel_x,
            train_panel_y,
            train_panel_width,
            train_panel_height,
            self.small_button_font,  # Используем тот же шрифт для кнопки и статуса
            button_text="Start Training",
            status_text="Status: Idle",
            colors=self.loader.getTrainingPanelColors(),
            action=self.toggle_training_action,
        )
        self.ui_elements.append(self.training_panel)

    def get_palette_items_data(self):
        """
        Формирует и возвращает данные для элементов палитры выбора объектов.
        Каждый элемент описывается словарем, содержащим тип, цвет и иконку.

        Args:
            None: Ничего не принимает.
            
        Returns:
            list: Список словарей, где каждый словарь представляет элемент палитры.
                  Пример элемента: `{'type': 'peaceful', 'color': (R,G,B), 'icon': None}`.
        """
        palette_colors = self.loader.getPaletteColors()
        return [
            {"type": "peaceful", "color": palette_colors["peaceful"], "icon": None},
            {"type": "predator", "color": palette_colors["predator"], "icon": None},
            {"type": "terrain", "color": palette_colors["terrain"], "icon": None},
            {"type": "food", "color": palette_colors["food"], "icon": None},
            {"type": "lake", "color": palette_colors["lake"], "icon": None},
            {"type": "obstacle", "color": palette_colors["obstacle"], "icon": None},
        ]

    def kill_func(self):
        """
        Останавливает процесс симуляции и обучения агентов.
        Устанавливает флаг `map_maybe_updated` в `False`, получает текущую эпоху,
        отправляет команду остановки в `sim_api` и разблокирует режим кисти на карте.
        
        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        self.game_state.map_maybe_updated = False
        current_epoch = self.sim_api.get_epoch()
        self.logger.info(f"Stop learning after {current_epoch} epochs.")
        stop_response = self.sim_api.stop(timeout=10)
        self.map_view.unlock_brush_mode()
        self.logger.info(f"Stop command response: {stop_response}")

    def start_func(self):
        """
        Запускает процесс симуляции и обучения агентов.
        Инициализирует `SimulationAPI`, запускает симуляцию, блокирует режим кисти на карте,
        устанавливает флаг `map_maybe_updated` в `True` и запускает поток для загрузки карты.
        
        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        self.logger.info("Training is starting")
        self.sim_api = SimulationAPI(self.game_state.map_data, self.agents_state)
        self.sim_api.start()
        self.map_view.ban_brush_mode()
        self.game_state.map_maybe_updated = True
        threading.Thread(target=self.map_loader).start()
        self.logger.info("Training has been started")

    def stop_action(self):
        """
        Обрабатывает действие остановки симуляции (например, по нажатию кнопки "Stop").
        Если симуляция запущена, обновляет статус на панели обучения,
        запускает `kill_func` в отдельном потоке и устанавливает `is_running` в `False`.
        
        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        if self.game_state.is_running:
            self.training_panel.set_status("Status: Idle")
            threading.Thread(target=self.kill_func).start()
            self.game_state.is_running = False

    def reset_game_action(self):
        """
        Обрабатывает действие сброса игровой карты.
        Если карта не обновляется симуляцией (`map_maybe_updated` равен `False`),
        заполняет данные карты нулями (очищает карту).
        
        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        self.logger.info("Action: Reset game requested")
        if not self.game_state.map_maybe_updated:
            self.game_state.map_data.fill(0)

    def select_tool_action(self, selected_type):
        """
        Обрабатывает выбор инструмента (цвета/типа объекта) из палитры.
        Устанавливает текущий выбранный цвет в `game_state` и обновляет
        цвет кисти в `map_view`.

        Args:
            selected_type (str): Строковый ключ выбранного типа объекта (например, "peaceful", "terrain").

        Returns:
            None: Ничего не возвращает.
        """
        self.game_state.current_color = self.loader.getPaletteColors()[selected_type]
        self.logger.info(f"Action: Tool selected: {selected_type}")
        self.map_view.set_current_color(self.game_state.current_color)

    def set_compute_mode_action(self, mode):
        """
        Обрабатывает переключение режима вычислений (CPU/GPU).
        Обновляет режим вычислений в `game_state` и соответствующее устройство
        в `agents_state` (конфигурация агентов).

        Args:
            mode (str): Новый режим вычислений. Ожидаются значения "CPU" или "GPU".

        Returns:
            None: Ничего не возвращает.
        """
        self.game_state.compute_mode = mode
        self.agents_state.DEVICE = (
            "cuda"
            if CUDA_IS_AVAILABLE and self.game_state.compute_mode == "GPU"
            else "cpu"
        )
        self.logger.warning(
            f"Action: Compute mode set to: {self.game_state.compute_mode}"
        )

    def toggle_training_action(self, start_training):
        """
        Обрабатывает действие запуска или остановки процесса обучения агентов.
        В текущей реализации обрабатывает только запуск обучения.

        Args:
            start_training (bool): Флаг, указывающий, следует ли начать обучение.
                                   `True` для начала обучения.

        Returns:
            None: Ничего не возвращает.
        """
        if start_training:
            self.logger.info("Action: Start background training")
            self.game_state.is_running = True
            threading.Thread(target=self.start_func).start()

    def map_loader(self):
        """
        Выполняется в отдельном потоке для периодического обновления данных карты.
        Пока установлен флаг `map_maybe_updated`, запрашивает снимок карты
        из `sim_api` и обновляет `game_state.map_data`.
        
        Args:
            None: Ничего не принимает.

        Returns:
            None: Ничего не возвращает.
        """
        while self.game_state.map_maybe_updated:
            self.game_state.map_data[:] = self.sim_api.get_map_snapshot()
            time.sleep(0.1)
