from ai_life_simulator._front.assets import *
import math
import pygame


# --- Константы и переменные ---
CLICK_MOVE_THRESHOLD = 5


class MapView:
    MODE_DRAG = "drag"
    MODE_PAINT = "paint"

    """
    Класс для создания и управления интерактивной картой в Pygame.

    Возможно изменять размер, перемещать облать видимости, и рисовать на карте
    Реагирует на наведение и нажатие мыши.
    """

    def __init__(
        self, viewport_rect, map_width_tiles, map_height_tiles, loader, map_storage
    ):
        """
        Инициализирует вид карты с viewport, кнопками режимов и зум-слайдером.

        Гарантирует, что начальный зум достаточен для заполнения viewport.

        Args:
            viewport_rect (pygame.Rect): Прямоугольник на экране, определяющий область видимости карты.
            map_width_tiles (int): Ширина карты в тайлах.
            map_height_tiles (int): Высота карты в тайлах.
            loader (StaticLoader): Объект для загрузки ресурсов (иконки, логгер).
            map_storage (list): Двумерный список (массив) данных карты, представляющий тайлы.
        """
        self.logger = StaticLoader.getLogger(__name__)
        self.logger.info("Start map initializing")

        self.viewport_rect = viewport_rect
        # Гарантируем, что размеры карты не нулевые, чтобы избежать деления на ноль
        self.map_width_tiles = max(1, map_width_tiles)
        self.map_height_tiles = max(1, map_height_tiles)
        self.draw_color = BROWN
        self.decode = StaticLoader.getPaletteCodes()
        self.mode = self.MODE_DRAG

        # --- Зум ---
        initial_tile_size = 30
        self.min_tile_size = 10  # Базовый минимальный размер
        self.max_tile_size = 60
        # Рассчитываем эффективный минимальный размер
        self.effective_min_tile_size = self._calculate_effective_min_tile_size()
        # Применяем ограничения к начальному размеру
        self.tile_size = max(
            self.effective_min_tile_size, min(initial_tile_size, self.max_tile_size)
        )
        self.logger.info(
            f"Initial tile_size calculation: requested={initial_tile_size}, "
            f"effective_min={self.effective_min_tile_size}, max={self.max_tile_size} -> final={self.tile_size}"
        )

        self.map_data = map_storage

        # --- Состояния для клика/перетаскивания ---
        self.paint_locked = False
        self.dragging = False
        self.potential_click = False
        self.mouse_down_pos = None
        self.start_drag_camera_pos = None

        # --- Состояния для слайдера зума ---
        self.is_dragging_slider = False
        self.slider_handle_offset_y = 0

        # --- Инициализация зависимых от tile_size параметров ---
        self.camera_x = 0
        self.camera_y = 0
        self.map_width_px = 0
        self.map_height_px = 0
        self.max_camera_x = 0
        self.max_camera_y = 0
        # Вызываем _update_zoom_dependent_vars ПОСЛЕ установки начального tile_size
        self._update_zoom_dependent_vars(center_view=True)

        # --- UI Элементы (Кнопки режима, Слайдер зума) ---
        self._setup_ui_elements(loader)

        self.logger.info("Map has been created")

    def _calculate_effective_min_tile_size(self):
        """
        Рассчитывает минимальный размер тайла (tile_size), необходимый для полного заполнения области просмотра (viewport).

        Этот размер гарантирует, что карта покроет всю область viewport даже при минимальном зуме,
        учитывая при этом базовый минимальный размер тайла.

        Returns:
            int: Эффективный минимальный размер тайла.
        """
        safe_map_width = max(1, self.map_width_tiles)
        safe_map_height = max(1, self.map_height_tiles)

        min_req_w = math.ceil(self.viewport_rect.width / safe_map_width)
        min_req_h = math.ceil(self.viewport_rect.height / safe_map_height)

        min_fill_tile_size = max(min_req_w, min_req_h)

        effective_min = max(self.min_tile_size, min_fill_tile_size)
        self.logger.debug(
            f"Calculated effective min tile size: "
            f"min_w={min_req_w}, min_h={min_req_h}, "
            f"min_fill={min_fill_tile_size}, base_min={self.min_tile_size} -> effective={effective_min}"
        )
        return effective_min

    def _setup_ui_elements(self, loader):
        """
        Инициализирует элементы пользовательского интерфейса, такие как кнопки режима и слайдер зума.

        Размеры и положения этих элементов рассчитываются на основе размеров viewport и других констант.

        Args:
            loader (StaticLoader): Объект для загрузки ресурсов (иконок).
        """
        button_size = int(self.max_tile_size * 0.8)
        button_padding = 5
        self.button_border_radius = 3
        self.button_bg_color = GRAY
        self.button_bg_active_color = MEDIUM_GRAY

        icon_inner_size = int(button_size * 0.7)
        self.drag_icon = pygame.transform.smoothscale(
            loader.getDragIcon(), (icon_inner_size, icon_inner_size)
        )
        self.paint_icon = pygame.transform.smoothscale(
            loader.getBrushIcon(), (icon_inner_size, icon_inner_size)
        )

        drag_button_x = self.viewport_rect.left + button_padding
        drag_button_y = self.viewport_rect.bottom - button_padding - button_size
        self.drag_button_rect = pygame.Rect(
            drag_button_x, drag_button_y, button_size, button_size
        )

        paint_button_x = drag_button_x + button_size + button_padding
        self.paint_button_rect = pygame.Rect(
            paint_button_x, drag_button_y, button_size, button_size
        )

        # --- Слайдер зума ---
        slider_padding = 15
        slider_width = 12
        slider_height = 150
        self.slider_handle_width = slider_width + 10
        self.slider_handle_height = 10
        self.slider_border_radius = 4

        slider_x = self.viewport_rect.left + slider_padding
        slider_y = self.viewport_rect.top + slider_padding
        self.slider_track_rect = pygame.Rect(
            slider_x, slider_y, slider_width, slider_height
        )

        handle_y = self._value_to_y(self.tile_size)
        handle_x = self.slider_track_rect.centerx - self.slider_handle_width // 2
        self.slider_handle_rect = pygame.Rect(
            handle_x,
            handle_y - self.slider_handle_height // 2,
            self.slider_handle_width,
            self.slider_handle_height,
        )

    def _update_zoom_dependent_vars(self, center_view=False, zoom_point_map=None):
        """
        Пересчитывает переменные, зависящие от размера тайла (tile_size), и обновляет положение камеры.

        Принудительно устанавливает tile_size в допустимые границы перед расчетами.
        Обновляет размеры карты в пикселях, максимальное смещение камеры и, при необходимости, центрирует вид.

        Args:
            center_view (bool, optional): Если True, камера будет отцентрирована после обновления. По умолчанию False.
            zoom_point_map (tuple, optional): Не используется в текущей реализации, но зарезервирован для
                                               масштабирования относительно точки на карте. По умолчанию None.
        """
        # 1. Пересчитываем эффективный минимальный размер тайла
        self.effective_min_tile_size = self._calculate_effective_min_tile_size()

        # 2. Принудительно ограничиваем текущий self.tile_size
        clamped_tile_size = max(
            self.effective_min_tile_size, min(self.tile_size, self.max_tile_size)
        )
        if clamped_tile_size != self.tile_size:
            self.logger.warning(
                f"Tile size {self.tile_size} was outside bounds "
                f"[{self.effective_min_tile_size}, {self.max_tile_size}]. Clamped to {clamped_tile_size}"
            )
            self.tile_size = clamped_tile_size

        # 3. Рассчитываем размеры карты в пикселях и максимальное смещение камеры
        self.map_width_px = self.map_width_tiles * self.tile_size
        self.map_height_px = self.map_height_tiles * self.tile_size

        self.max_camera_x = max(0, self.map_width_px - self.viewport_rect.width)
        self.max_camera_y = max(0, self.map_height_px - self.viewport_rect.height)

        # 4. Обновляем позицию камеры
        if center_view:
            self.camera_x = self.max_camera_x / 2
            self.camera_y = self.max_camera_y / 2

        else:
            self.camera_x = max(0, min(self.camera_x, self.max_camera_x))
            self.camera_y = max(0, min(self.camera_y, self.max_camera_y))

        # 5. Обновляем позицию ползунка слайдера (на случай, если tile_size был скорректирован)
        if hasattr(self, "slider_handle_rect"):
            new_handle_y = self._value_to_y(self.tile_size)
            self.slider_handle_rect.centery = new_handle_y

    def get_tile_indices_at_pos(self, screen_pos):
        """
        Возвращает индексы (строка, столбец) тайла под указанной экранной позицией.

        Если позиция находится вне области viewport, над UI элементом, или если tile_size некорректен,
        возвращает None.

        Args:
            screen_pos (tuple[int, int]): Координаты (x, y) на экране.

        Returns:
            tuple[int, int] | None: Кортеж (строка, столбец) индексов тайла или None, если тайл не найден.
        """
        if (
            not self.viewport_rect.collidepoint(screen_pos)
            or self.drag_button_rect.collidepoint(screen_pos)
            or self.paint_button_rect.collidepoint(screen_pos)
            or self.slider_track_rect.collidepoint(screen_pos)
            or self.slider_handle_rect.collidepoint(screen_pos)
        ):
            return None

        if self.tile_size <= 0:
            return None
        map_coord_x = screen_pos[0] - self.viewport_rect.x + self.camera_x
        map_coord_y = screen_pos[1] - self.viewport_rect.y + self.camera_y

        tile_col = int(map_coord_x // self.tile_size)
        tile_row = int(map_coord_y // self.tile_size)

        if (
            0 <= tile_row < self.map_height_tiles
            and 0 <= tile_col < self.map_width_tiles
        ):
            return tile_row, tile_col
        return None

    def _y_to_value(self, y):
        """
        Преобразует Y координату центра ползунка в значение tile_size.
        
        Args:
            y (int | float): Y-координата центра ползунка на треке слайдера.

        Returns:
            int: Соответствующее значение tile_size, округленное до целого.
        """
        self.logger.debug(f"--- _y_to_value ---")
        self.logger.debug(f"Input y: {y}")

        value_range = self.max_tile_size - self.effective_min_tile_size
        if value_range <= 0:
            return self.effective_min_tile_size

        relative_y = (y - self.slider_track_rect.top) / self.slider_track_rect.height
        clamped_relative_y = 1.0 - max(0.0, min(1.0, relative_y))
        value = self.effective_min_tile_size + clamped_relative_y * value_range

        self.logger.debug(f"Rounded integer value: {value}")
        self.logger.debug(f"--- _y_to_value End ---")
        return int(round(value))

    def _value_to_y(self, value):
        """
        Преобразует значение tile_size в Y координату центра ползунка.
        
        Args:
            value (int | float): Значение tile_size.

        Returns:
            int: Соответствующая Y-координата центра ползунка на треке слайдера, округленная до целого.
        """
        self.logger.debug(f"--- _value_to_y ---")
        self.logger.debug(f"Input value: {value}")

        value_range = self.max_tile_size - self.effective_min_tile_size
        if value_range <= 0:
            return self.slider_track_rect.bottom

        clamped_value = max(
            self.effective_min_tile_size, min(value, self.max_tile_size)
        )
        relative_value = (clamped_value - self.effective_min_tile_size) / value_range
        relative_y = 1.0 - relative_value
        y = self.slider_track_rect.top + relative_y * self.slider_track_rect.height
        self.logger.debug(f"Rounded integer y: {y}")
        self.logger.debug(f"--- _value_to_y End ---")
        return int(round(y))

    def ban_brush_mode(self):
        """Запрещаем рисовать на карте"""
        self.mode = self.MODE_DRAG
        self.paint_locked = True

    def unlock_brush_mode(self):
        """Разрешаем рисовать на карте"""
        self.paint_locked = False

    def handle_event(self, event):
        """Обрабатывает события мыши для UI, перетаскивания, рисования, зума.
        
        Args:
            event (pygame.event.Event): Событие Pygame для обработки.

        Returns:
            bool: True, если событие было обработано этим методом, иначе False.
        """
        self.logger.debug(f"--- handle_event: Received event: {event}")
        mouse_pos = pygame.mouse.get_pos()
        event_handled = False

        # --- 1. Обработка слайдера зума ---
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if self.slider_handle_rect.collidepoint(mouse_pos):
                self.logger.info(
                    f"Slider handle clicked at {mouse_pos}. Starting drag."
                )
                self.is_dragging_slider = True
                self.slider_handle_offset_y = mouse_pos[1] - self.slider_handle_rect.top
                event_handled = True
            elif self.slider_track_rect.collidepoint(mouse_pos):
                self.logger.info(
                    f"Slider track clicked at {mouse_pos}. Jumping handle and starting drag."
                )

                new_center_y = mouse_pos[1]
                clamped_center_y = max(
                    self.slider_track_rect.top,
                    min(new_center_y, self.slider_track_rect.bottom),
                )
                new_tile_size = self._y_to_value(clamped_center_y)

                if new_tile_size != self.tile_size:
                    self.tile_size = new_tile_size
                    self._update_zoom_dependent_vars()
                else:
                    self.slider_handle_rect.centery = clamped_center_y

                self.is_dragging_slider = True
                self.slider_handle_offset_y = mouse_pos[1] - self.slider_handle_rect.top
                event_handled = True

        if self.is_dragging_slider:
            if event.type == pygame.MOUSEMOTION:
                self.logger.debug(f"Slider drag motion to {mouse_pos}")
                new_center_y = (
                    mouse_pos[1]
                    - self.slider_handle_offset_y
                    + self.slider_handle_height // 2
                )
                clamped_center_y = max(
                    self.slider_track_rect.top,
                    min(new_center_y, self.slider_track_rect.bottom),
                )
                new_tile_size = self._y_to_value(clamped_center_y)

                # Если значение изменилось, обновляем все
                if new_tile_size != self.tile_size:
                    self.tile_size = new_tile_size
                    self._update_zoom_dependent_vars()
                else:
                    # Если значение не изменилось, но мышь движется, просто двигаем ползунок
                    self.slider_handle_rect.centery = clamped_center_y

                event_handled = True

            elif event.type == pygame.MOUSEBUTTONUP and event.button == 1:
                self.logger.info(f"Slider drag ended at {mouse_pos}")
                self.is_dragging_slider = False
                # При отпускании кнопки окончательно ставим ползунок по значению
                self.slider_handle_rect.centery = self._value_to_y(self.tile_size)
                event_handled = True  # Захватываем событие

        # --- 2. Обработка кнопок режимов (если событие не обработано слайдером) ---
        if (
            not event_handled
            and event.type == pygame.MOUSEBUTTONUP
            and event.button == 1
        ):
            if self.drag_button_rect.collidepoint(mouse_pos):
                if self.mode != self.MODE_DRAG:
                    self.logger.info("Switching to DRAG mode")
                    self.mode = self.MODE_DRAG
                event_handled = True
            elif self.paint_button_rect.collidepoint(mouse_pos):
                if self.mode != self.MODE_PAINT and not self.paint_locked:
                    self.logger.info("Switching to PAINT mode")
                    self.mode = self.MODE_PAINT
                event_handled = True

        # --- 3. Обработка перетаскивания и рисования (если событие не обработано UI) ---
        if not event_handled:
            if event.type == pygame.MOUSEBUTTONDOWN:
                if (
                    self.viewport_rect.collidepoint(event.pos)
                    and not self.drag_button_rect.collidepoint(event.pos)
                    and not self.paint_button_rect.collidepoint(event.pos)
                    and not self.slider_track_rect.collidepoint(event.pos)
                    and not self.slider_handle_rect.collidepoint(event.pos)
                ):
                    if event.button == 1:
                        self.logger.debug(
                            f"Map area clicked (Button {event.button}) at {event.pos}. Mode: {self.mode}"
                        )
                        if self.mode == self.MODE_DRAG:
                            self.dragging = True
                            self.potential_click = False
                            self.mouse_down_pos = event.pos
                            self.start_drag_camera_pos = (self.camera_x, self.camera_y)
                            self.logger.info(
                                f"   Starting DRAG. Start camera pos: {self.start_drag_camera_pos}"
                            )
                        elif self.mode == self.MODE_PAINT:
                            self.dragging = True
                            self.logger.info(
                                f"   Starting PAINT interaction. potential_click=True"
                            )
                            self.potential_click = True
                            self.mouse_down_pos = event.pos

                            tile_indices = self.get_tile_indices_at_pos(event.pos)
                            if tile_indices:
                                tile_row, tile_col = tile_indices
                                if (
                                    self.decode[self.map_data[tile_row][tile_col]]
                                    != self.draw_color
                                ):
                                    self.map_data[tile_row][tile_col] = self.decode[
                                        self.draw_color
                                    ][0]
                                    # self.logger.debug(f"Painted tile on click: ({tile_row}, {tile_col})")
            elif event.type == pygame.MOUSEMOTION:
                if self.dragging:
                    current_mouse_pos = event.pos
                    if self.potential_click:
                        dx = current_mouse_pos[0] - self.mouse_down_pos[0]
                        dy = current_mouse_pos[1] - self.mouse_down_pos[1]
                        distance_sq = dx * dx + dy * dy
                        if distance_sq > CLICK_MOVE_THRESHOLD * CLICK_MOVE_THRESHOLD:
                            self.potential_click = False

                    if self.mode == self.MODE_DRAG and not self.potential_click:
                        drag_dx = current_mouse_pos[0] - self.mouse_down_pos[0]
                        drag_dy = current_mouse_pos[1] - self.mouse_down_pos[1]
                        self.camera_x = max(
                            0,
                            min(
                                self.start_drag_camera_pos[0] - drag_dx,
                                self.max_camera_x,
                            ),
                        )
                        self.camera_y = max(
                            0,
                            min(
                                self.start_drag_camera_pos[1] - drag_dy,
                                self.max_camera_y,
                            ),
                        )
                        # self.logger.debug(f"Dragging camera to: ({int(self.camera_x)}, {int(self.camera_y)})")

                    elif self.mode == self.MODE_PAINT and not self.potential_click:
                        tile_indices = self.get_tile_indices_at_pos(event.pos)
                        if tile_indices:
                            tile_row, tile_col = tile_indices
                            if (
                                self.decode[self.map_data[tile_row][tile_col]]
                                != self.draw_color
                            ):
                                self.map_data[tile_row][tile_col] = self.decode[
                                    self.draw_color
                                ][0]
                                # self.logger.debug(f"Painted tile on drag: ({tile_row}, {tile_col})")
            elif event.type == pygame.MOUSEBUTTONUP:
                if event.button == 1:
                    if self.dragging:
                        self.dragging = False
                        self.potential_click = False
                        self.mouse_down_pos = None
                        self.start_drag_camera_pos = None
                        self.logger.info(
                            "   Reset interaction state: dragging=False, potential_click=False"
                        )

        # Обработка колеса мыши для зума
        if not event_handled and event.type == pygame.MOUSEWHEEL:
            zoom_factor = 1.1 if event.y > 0 else (1 / 1.1)
            new_tile_size_float = self.tile_size * zoom_factor
            new_tile_size = int(round(new_tile_size_float))
            new_tile_size = max(
                self.min_tile_size, min(new_tile_size, self.max_tile_size)
            )

            if new_tile_size != self.tile_size:
                self.tile_size = new_tile_size
                self._update_zoom_dependent_vars()

                event_handled = True

        return event_handled

    def set_current_color(self, color):
        """
        Устанавливает текущий цвет для рисования на карте.

        Args:
            color (tuple): RGB кортеж цвета, например, (255, 0, 0) для красного.
                           Ожидается, что это один из цветов, определенных в `assets.py`.
        """
        self.draw_color = color

    def draw(self, surface):
        """
        Отрисовывает видимую часть карты и UI элементы.
        
        Args:
            surface (pygame.Surface): Поверхность Pygame, на которой будет производиться отрисовка.
        """
        # --- 1. Отрисовка карты (с обрезкой по viewport) ---
        original_clip = surface.get_clip()
        surface.set_clip(self.viewport_rect)

        if self.tile_size > 0:
            start_col = int(self.camera_x // self.tile_size)
            start_row = int(self.camera_y // self.tile_size)
            end_col = (
                start_col + math.ceil(self.viewport_rect.width / self.tile_size) + 1
            )
            end_row = (
                start_row + math.ceil(self.viewport_rect.height / self.tile_size) + 1
            )
            end_col = min(end_col, self.map_width_tiles)
            end_row = min(end_row, self.map_height_tiles)

            for r in range(start_row, end_row):
                for c in range(start_col, end_col):
                    tile_screen_x = self.viewport_rect.x + (
                        c * self.tile_size - self.camera_x
                    )
                    tile_screen_y = self.viewport_rect.y + (
                        r * self.tile_size - self.camera_y
                    )
                    tile_rect = pygame.Rect(
                        tile_screen_x, tile_screen_y, self.tile_size, self.tile_size
                    )

                    if self.viewport_rect.colliderect(tile_rect):
                        pygame.draw.rect(
                            surface, self.decode[self.map_data[r][c]], tile_rect
                        )
                        if self.tile_size > 5:
                            pygame.draw.rect(surface, GRAY, tile_rect, 1)

        surface.set_clip(original_clip)

        # --- 2. Отрисовка UI (поверх карты) ---
        self._draw_ui(surface)

    def _draw_ui(self, surface):
        """
        Вспомогательный метод для отрисовки UI элементов.
        
        Args:
            surface (pygame.Surface): Поверхность Pygame, на которой будет производиться отрисовка UI.
        """
        drag_bg = (
            self.button_bg_active_color
            if self.mode == self.MODE_DRAG
            else self.button_bg_color
        )
        paint_bg = (
            self.button_bg_active_color
            if self.mode == self.MODE_PAINT
            else self.button_bg_color
        )

        pygame.draw.rect(
            surface,
            drag_bg,
            self.drag_button_rect,
            border_radius=self.button_border_radius,
        )
        if self.mode == self.MODE_DRAG:
            pygame.draw.rect(
                surface,
                DARK_GRAY,
                self.drag_button_rect,
                width=2,
                border_radius=self.button_border_radius,
            )

        pygame.draw.rect(
            surface,
            paint_bg,
            self.paint_button_rect,
            border_radius=self.button_border_radius,
        )
        if self.mode == self.MODE_PAINT:
            pygame.draw.rect(
                surface,
                DARK_GRAY,
                self.paint_button_rect,
                width=2,
                border_radius=self.button_border_radius,
            )

        drag_icon_rect = self.drag_icon.get_rect(center=self.drag_button_rect.center)
        surface.blit(self.drag_icon, drag_icon_rect)
        paint_icon_rect = self.paint_icon.get_rect(center=self.paint_button_rect.center)
        surface.blit(self.paint_icon, paint_icon_rect)

        pygame.draw.rect(
            surface,
            SLIDER_TRACK_COLOR,
            self.slider_track_rect,
            border_radius=self.slider_border_radius,
        )
        pygame.draw.rect(
            surface,
            SLIDER_HANDLE_COLOR,
            self.slider_handle_rect,
            border_radius=self.slider_border_radius // 2,
        )
        pygame.draw.rect(
            surface,
            DARK_GRAY,
            self.slider_handle_rect,
            width=1,
            border_radius=self.slider_border_radius // 2,
        )


class UIElement:
    """Базовый класс для элементов интерфейса."""

    def __init__(self, x, y, width, height):
        """
        Инициализирует базовый элемент UI.

        Args:
            x (int): Координата X верхнего левого угла элемента.
            y (int): Координата Y верхнего левого угла элемента.
            width (int): Ширина элемента.
            height (int): Высота элемента.
        """
        self.rect = pygame.Rect(x, y, width, height)
        self.logger = StaticLoader.getLogger(__name__)
        self.visible = True
        self.hovered = False

    def handle_event(self, event):
        """
        Обрабатывает событие Pygame для элемента UI.

        Обновляет состояние `hovered` при движении мыши.
        Вызывает `on_click` при нажатии левой кнопки мыши над элементом.

        Args:
            event (pygame.event.Event): Событие Pygame.

        Returns:
            bool: True, если событие было обработано (клик по элементу), иначе False.
        """
        if not self.visible:
            return False
        if event.type == pygame.MOUSEMOTION:
            self.hovered = self.rect.collidepoint(event.pos)
        elif event.type == pygame.MOUSEBUTTONDOWN:
            if self.hovered and event.button == 1:
                return self.on_click(event)
        return False

    def on_click(self, event):
        """
        Метод, вызываемый при клике на элемент.

        Должен быть переопределен в дочерних классах для выполнения специфических действий.

        Args:
            event (pygame.event.Event): Событие MOUSEBUTTONDOWN, вызвавшее клик.

        Returns:
            bool: True, если клик обработан. По умолчанию True.
        """
        self.logger.debug(f"{self.__class__.__name__} clicked at {event.pos}")
        return True

    def draw(self, surface):
        """
        Отрисовывает элемент UI на указанной поверхности.

        Должен быть переопределен в дочерних классах для конкретной отрисовки.

        Args:
            surface (pygame.Surface): Поверхность Pygame для отрисовки.
        """
        if not self.visible:
            return


class Button(UIElement):
    """Стилизованная кнопка с текстом и эффектом сдвига при нажатии."""

    def __init__(
        self,
        x,
        y,
        width,
        height,
        font,
        text="",
        colors=None,
        action=None,
        border_radius=5,
        text_color=(255, 255, 255),
        click_offset=3,
    ):
        """
        Инициализирует кнопку.

        Args:
            x (int): Координата X верхнего левого угла кнопки.
            y (int): Координата Y верхнего левого угла кнопки.
            width (int): Ширина кнопки.
            height (int): Высота кнопки.
            font (pygame.font.Font): Шрифт для текста кнопки.
            text (str, optional): Текст на кнопке. По умолчанию "".
            colors (dict, optional): Словарь цветов для состояний 'normal', 'hover', 'active'.
                                     Если None, используются цвета по умолчанию.
            action (callable, optional): Функция, вызываемая при нажатии кнопки. По умолчанию None.
            border_radius (int, optional): Радиус скругления углов кнопки. По умолчанию 5.
            text_color (tuple, optional): Цвет текста RGB. По умолчанию (255, 255, 255) (белый).
            click_offset (int, optional): Смещение текста и/или фона при "нажатом" состоянии. По умолчанию 3.
        """
        super().__init__(x, y, width, height)
        self.font = font
        self.text = text
        self.action = action
        self.border_radius = border_radius
        self.text_color = text_color
        self.click_offset = click_offset

        self.colors = colors or {
            "normal": (50, 100, 200),
            "hover": (70, 120, 220),
            "active": (40, 80, 180),
        }
        self.current_color = self.colors["normal"]
        self.is_active = False

        self._render_text()

    def _render_text(self):
        """Создает поверхность текста и ее rect."""
        if self.text:
            self.text_surface = self.font.render(self.text, True, self.text_color)
            self.text_rect = self.text_surface.get_rect(center=self.rect.center)
        else:
            self.text_surface = None
            self.text_rect = None

    def handle_event(self, event):
        """
        Обрабатывает события мыши для кнопки.

        Изменяет цвет кнопки при наведении и нажатии.
        Вызывает действие `action` при отпускании кнопки мыши над элементом.

        Args:
            event (pygame.event.Event): Событие Pygame.

        Returns:
            bool: True, если событие было связано с кнопкой (наведение, нажатие, отпускание), иначе False.
        """
        if not self.visible:
            return False

        was_hovered = self.hovered
        self.hovered = self.rect.collidepoint(
            event.pos if hasattr(event, "pos") else (-1, -1)
        )

        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if self.hovered:
                self.is_active = True
                self.current_color = self.colors.get("active", self.colors["normal"])
                return True
        elif event.type == pygame.MOUSEBUTTONUP and event.button == 1:
            if self.is_active:
                if self.hovered and self.action:
                    self.logger.info(f"Button {self.text} has been clicked")
                    self.action()
                self.is_active = False
                self.current_color = (
                    self.colors["hover"] if self.hovered else self.colors["normal"]
                )
                return True
        elif event.type == pygame.MOUSEMOTION:
            if not self.is_active:
                if self.hovered:
                    self.current_color = self.colors.get("hover", self.colors["normal"])
                elif was_hovered:
                    self.current_color = self.colors["normal"]

        if self.hovered and not self.is_active:
            self.current_color = self.colors.get("hover", self.colors["normal"])
        elif not self.hovered and not self.is_active:
            self.current_color = self.colors["normal"]

        return self.hovered

    def on_click(self, event):
        """
        Обработка клика для Button управляется в `handle_event` через MOUSEBUTTONDOWN/UP.
        Этот метод оставлен для совместимости с базовым `UIElement`, но не используется напрямую.

        Args:
            event (pygame.event.Event): Событие MOUSEBUTTONDOWN.

        Returns:
            bool: Всегда True.
        """
        return True

    def draw(self, surface):
        """
        Отрисовывает кнопку на указанной поверхности.

        Применяет эффект смещения, если кнопка активна (нажата).

        Args:
            surface (pygame.Surface): Поверхность Pygame для отрисовки.
        """
        if not self.visible:
            return

        # Определяем сдвиг
        offset = self.click_offset if self.is_active else 0

        # --- Рисуем фон ---
        # Создаем смещенный прямоугольник для фона
        pygame.draw.rect(
            surface, self.current_color, self.rect, border_radius=self.border_radius
        )

        if self.text_surface:
            text_pos = (self.text_rect.x + offset, self.text_rect.y + offset)
            surface.blit(self.text_surface, text_pos)

    def set_text(self, text):
        """
        Изменяет текст кнопки и перерисовывает его внутреннюю поверхность.

        Args:
            text (str): Новый текст для кнопки.
        """
        if text != self.text:
            self.text = text
            self._render_text()


class PaletteItem(UIElement):
    """Элемент палитры (цветной квадрат или иконка)."""

    def __init__(
        self,
        x,
        y,
        size,
        item_type,
        color,
        icon=None,
        selected_border_color=YELLOW,
        border_width=4,
    ):
        """
        Инициализирует элемент палитры.

        Args:
            x (int): Координата X верхнего левого угла элемента.
            y (int): Координата Y верхнего левого угла элемента.
            size (int): Размер стороны квадратного элемента.
            item_type (any): Уникальный идентификатор типа элемента (например, строка или enum).
            color (tuple): Цвет фона элемента (RGB).
            icon (pygame.Surface, optional): Иконка для отображения на элементе. По умолчанию None.
            selected_border_color (tuple, optional): Цвет рамки выбранного элемента (RGB).
                                                    По умолчанию YELLOW.
            border_width (int, optional): Толщина рамки выбранного элемента. По умолчанию 4.
        """
        super().__init__(x, y, size, size)
        self.item_type = item_type
        self.color = color
        self.icon = icon
        self.selected_border_color = selected_border_color
        self.border_width = border_width
        self.selected = False

    def on_click(self, event):
        """
        Обрабатывает клик по элементу палитры.

        Args:
            event (pygame.event.Event): Событие MOUSEBUTTONDOWN.

        Returns:
            bool: Всегда True, указывая, что клик был по этому элементу.
        """
        return True

    def draw(self, surface):
        """
        Отрисовывает элемент палитры на указанной поверхности.

        Отображает фон, иконку (если есть), рамку выбора (если выбран)
        и эффект наведения мыши.

        Args:
            surface (pygame.Surface): Поверхность Pygame для отрисовки.
        """
        if not self.visible:
            return

        pygame.draw.rect(surface, self.color, self.rect)

        if self.icon:
            icon_rect = self.icon.get_rect(center=self.rect.center)
            surface.blit(self.icon, icon_rect)

        if self.selected:
            border_width = self.border_width
            pygame.draw.rect(
                surface, self.selected_border_color, self.rect, border_width
            )

        if self.hovered and not self.selected:
            s = pygame.Surface(self.rect.size, pygame.SRCALPHA)
            s.fill((255, 255, 255, 50))
            surface.blit(s, self.rect.topleft)


class Palette(UIElement):
    """Контейнер для элементов палитры."""

    def __init__(
        self,
        x,
        y,
        width,
        height,
        items_data,
        cols=3,
        item_size=30,
        margin=5,
        on_select=None,
    ):
        """
        Инициализирует палитру.

        Args:
            x (int): Координата X верхнего левого угла палитры.
            y (int): Координата Y верхнего левого угла палитры.
            width (int): Ширина области палитры.
            height (int): Высота области палитры.
            items_data (list[dict]): Список словарей, описывающих каждый элемент палитры.
                                     Каждый словарь должен содержать 'type', 'color', и опционально 'icon'.
            cols (int, optional): Количество столбцов для расположения элементов. По умолчанию 3.
            item_size (int, optional): Размер каждого элемента палитры. По умолчанию 30.
            margin (int, optional): Отступ между элементами палитры. По умолчанию 5.
            on_select (callable, optional): Функция, вызываемая при выборе элемента.
                                           Принимает `item_type` выбранного элемента. По умолчанию None.
        """
        super().__init__(x, y, width, height)
        self.items_data = items_data
        self.cols = cols
        self.item_size = item_size
        self.margin = margin
        self.on_select = on_select
        self.items = []
        self.selected_item_type = None
        self._create_items()

    def _create_items(self):
        """
        Создает объекты PaletteItem на основе `items_data` и располагает их в сетке.
        """
        current_x = self.rect.left
        current_y = self.rect.top
        col_count = 0
        for data in self.items_data:
            item = PaletteItem(
                current_x,
                current_y,
                self.item_size,
                data["type"],
                data["color"],
                data.get("icon"),
            )
            self.items.append(item)

            current_x += self.item_size + self.margin
            col_count += 1
            if col_count >= self.cols:
                col_count = 0
                current_x = self.rect.left
                current_y += self.item_size + self.margin

    def handle_event(self, event):
        """
        Обрабатывает события для палитры и ее элементов.

        Передает события каждому элементу PaletteItem. Если клик происходит на элементе,
        этот элемент выбирается, и вызывается `on_select`.

        Args:
            event (pygame.event.Event): Событие Pygame.

        Returns:
            bool: True, если событие было обработано палитрой или одним из ее элементов, иначе False.
        """
        if not self.visible:
            return False

        clicked_item = None
        for item in self.items:
            if item.handle_event(event):
                if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                    clicked_item = item
                    break

        if clicked_item:
            self.select_item(clicked_item.item_type)
            return True

        # Передаем MOUSEMOTION остальным для обновления hover состояния
        if event.type == pygame.MOUSEMOTION:
            for item in self.items:
                item.handle_event(event)
            return self.rect.collidepoint(event.pos)

        return False

    def select_item(self, item_type):
        """
        Выбирает элемент палитры по его типу.

        Устанавливает флаг `selected` для выбранного элемента и снимает его с других.
        Вызывает колбэк `on_select`, если он был предоставлен.

        Args:
            item_type (any): Тип элемента для выбора.
        """
        if self.selected_item_type == item_type:
            return
        else:
            self.selected_item_type = item_type
            for item in self.items:
                item.selected = item.item_type == item_type

        self.logger.info(f"Palette selected: {self.selected_item_type}")
        if self.on_select:
            self.on_select(self.selected_item_type)

    def draw(self, surface):
        """
        Отрисовывает все видимые элементы палитры на указанной поверхности.

        Args:
            surface (pygame.Surface): Поверхность Pygame для отрисовки.
        """
        if not self.visible:
            return
        for item in self.items:
            item.draw(surface)


class Switch(UIElement):
    """
    Переключатель с двумя состояниями и необязательной меткой под ним,
    отображающей текущее выбранное состояние.
    """

    def __init__(
        self,
        x,
        y,
        width,
        height,
        label_font,
        labels=("CPU", "GPU"),
        colors=None,
        action=None,
        initial_state=False,
        border_radius=None,
        label_color=MEDIUM_GRAY,
        label_margin=5,
    ):
        """
        Инициализирует переключатель.

        Args:
            x (int): Координата X верхнего левого угла переключателя.
            y (int): Координата Y верхнего левого угла переключателя.
            width (int): Ширина переключателя.
            height (int): Высота переключателя.
            label_font (pygame.font.Font): Шрифт для метки состояния.
            labels (tuple[str, str], optional): Кортеж из двух строк для состояний (False, True).
                                                По умолчанию ("CPU", "GPU").
            colors (dict, optional): Словарь цветов для 'bg_off', 'bg_on', 'knob', 'knob_hover'.
                                     Если None, используются цвета по умолчанию.
            action (callable, optional): Функция, вызываемая при изменении состояния.
                                         Принимает новую метку состояния (одну из `labels`). По умолчанию None.
            initial_state (bool, optional): Начальное состояние переключателя (False или True).
                                            По умолчанию False.
            border_radius (int, optional): Радиус скругления углов фона. Если None, равен половине высоты.
            label_color (tuple, optional): Цвет текста метки состояния (RGB). По умолчанию MEDIUM_GRAY.
            label_margin (int, optional): Отступ метки состояния от нижней границы переключателя. По умолчанию 5.
        """
        super().__init__(x, y, width, height)
        self.labels = labels
        self.state = initial_state
        self.action = action
        self.border_radius = (
            border_radius if border_radius is not None else int(height // 2)
        )
        self.knob_radius = height // 2 - 3
        self.knob_x_off = self.rect.left + self.border_radius
        self.knob_x_on = self.rect.right - self.border_radius - 1

        self.colors = colors or {
            "bg_off": (100, 100, 100),
            "bg_on": (100, 180, 100),
            "knob": (220, 220, 220),
            "knob_hover": (255, 255, 255),
        }
        self.current_knob_color = self.colors["knob"]

        self.label_rect = None
        self.label_font = None
        self.label_color = label_color
        self.label_margin = label_margin
        self.label_font = label_font

    def on_click(self, event):
        """
        Обрабатывает клик по переключателю.

        Изменяет состояние переключателя (self.state) и вызывает действие `action`
        с новой меткой состояния.

        Args:
            event (pygame.event.Event): Событие MOUSEBUTTONDOWN.

        Returns:
            bool: Всегда True, указывая, что клик обработан.
        """
        self.state = not self.state
        self.logger.info(f"Switch toggled to: {self.labels[int(self.state)]}")
        if self.action:
            self.action(self.labels[int(self.state)])
        return True

    def handle_event(self, event):
        """
        Обрабатывает события мыши для переключателя.

        Обновляет цвет ползунка при наведении и вызывает `on_click` при нажатии.

        Args:
            event (pygame.event.Event): Событие Pygame.

        Returns:
            bool: True, если событие было связано с переключателем, иначе False.
        """
        if not self.visible:
            return False

        was_hovered = self.hovered

        if event.type == pygame.MOUSEMOTION:
            mouse_pos = pygame.mouse.get_pos()
            self.hovered = self.rect.collidepoint(mouse_pos)
            if self.hovered:
                self.current_knob_color = self.colors.get(
                    "knob_hover", self.colors["knob"]
                )
            elif was_hovered:
                self.current_knob_color = self.colors["knob"]

        handled = super().handle_event(event)

        # Если не было MOUSEMOTION, но мышь ушла с элемента (например, окно потеряло фокус)
        if not self.hovered and self.current_knob_color != self.colors["knob"]:
            self.current_knob_color = self.colors["knob"]

        return handled

    def draw(self, surface):
        """
        Отрисовывает переключатель на указанной поверхности.

        Включает фон, ползунок и текстовую метку текущего состояния.

        Args:
            surface (pygame.Surface): Поверхность Pygame для отрисовки.
        """
        if not self.visible:
            return

        bg_color = self.colors["bg_on"] if self.state else self.colors["bg_off"]
        pygame.draw.rect(surface, bg_color, self.rect, border_radius=self.border_radius)

        knob_center_x = self.knob_x_on if self.state else self.knob_x_off
        knob_center_y = self.rect.centery
        pygame.draw.circle(
            surface,
            self.current_knob_color,
            (knob_center_x, knob_center_y),
            self.knob_radius,
        )

        if self.label_rect:
            surface.fill((241, 241, 241), self.label_rect)
        current_label_text = self.labels[int(self.state)]
        label_surface = self.label_font.render(
            current_label_text, True, self.label_color
        )
        self.label_rect = label_surface.get_rect()
        self.label_rect.centerx = self.rect.centerx
        self.label_rect.top = self.rect.bottom + self.label_margin
        surface.blit(label_surface, self.label_rect)


class Label(UIElement):
    """Простой текстовый элемент."""

    def __init__(
        self,
        x,
        y,
        width,
        height,
        font,
        text="",
        text_color=(200, 200, 200),
        bg_color=None,
        alignment="center",
    ):
        """
        Инициализирует текстовую метку.

        Args:
            x (int): Начальная координата X. Позиция будет скорректирована выравниванием.
            y (int): Начальная координата Y. Позиция будет скорректирована выравниванием.
            width (int): Начальная ширина области для текста. Может измениться.
            height (int): Начальная высота области для текста. Может измениться.
            font (pygame.font.Font): Шрифт для текста.
            text (str, optional): Отображаемый текст. По умолчанию "".
            text_color (tuple, optional): Цвет текста (RGB). По умолчанию (200, 200, 200).
            bg_color (tuple, optional): Цвет фона метки (RGB). Если None, фон прозрачный.
                                        По умолчанию None.
            alignment (str, optional): Выравнивание текста внутри исходного `rect`.
                                       Варианты: 'left', 'center', 'right'. По умолчанию 'center'.
        """
        super().__init__(x, y, width, height)
        self.font = font
        self.text = text
        self.text_is_set = False
        self.text_color = text_color
        self.bg_color = bg_color
        self.alignment = alignment  # 'left', 'center', 'right'
        self._render_text()

    def _render_text(self):
        """
        Создает поверхность с текстом и обновляет прямоугольник элемента.
        Вызывается при инициализации и при изменении текста.
        """
        self.text_surface = self.font.render(self.text, True, self.text_color)
        self.text_rect = self.text_surface.get_rect()

        if self.alignment == "center":
            self.text_rect.center = self.rect.center
        elif self.alignment == "left":
            self.text_rect.midleft = self.rect.midleft
        elif self.alignment == "right":
            self.text_rect.midright = self.rect.midright
        else:
            self.text_rect.center = self.rect.center
        self.rect = self.text_rect

    def set_text(self, text):
        """
        Изменяет текст метки.

        Текст будет обновлен при следующей отрисовке.

        Args:
            text (str): Новый текст для метки.
        """
        if text != self.text:
            self.text = text
            self.text_is_set = True

    def handle_event(self, event):
        """
        Метки не обрабатывают события.

        Args:
            event (pygame.event.Event): Событие Pygame.

        Returns:
            bool: Всегда False.
        """
        return False

    def draw(self, surface):
        """
        Отрисовывает текстовую метку на указанной поверхности.

        Если текст был изменен, он перерисовывается.

        Args:
            surface (pygame.Surface): Поверхность Pygame для отрисовки.
        """
        if not self.visible:
            return

        if self.text_is_set:
            if self.bg_color:
                pygame.draw.rect(surface, self.bg_color, self.rect)
            self._render_text()
            self.text_is_set = False

        surface.blit(self.text_surface, self.text_rect)


class TrainingPanel(UIElement):
    """Панель для управления обучением."""

    def __init__(
        self,
        x,
        y,
        width,
        height,
        font,
        button_text="Start Training",
        status_text="Status: Idle",
        colors=None,
        action=None,
    ):
        """
        Инициализирует панель управления обучением.

        Args:
            x (int): Координата X верхнего левого угла панели.
            y (int): Координата Y верхнего левого угла панели.
            width (int): Ширина панели.
            height (int): Высота панели.
            font (pygame.font.Font): Шрифт для кнопки и метки статуса.
            button_text (str, optional): Начальный текст на кнопке. По умолчанию "Start Training".
            status_text (str, optional): Начальный текст метки статуса. По умолчанию "Status: Idle".
            colors (dict, optional): Словарь с настройками цветов для 'button' и 'label'.
                                     Например: {'button': {'normal': ..., 'hover': ...}, 'label': {'text': ..., 'bg': ...}}
                                     По умолчанию используются стандартные цвета дочерних элементов.
            action (callable, optional): Функция, вызываемая при нажатии кнопки.
                                         Принимает bool (True, если обучение запускается). По умолчанию None.
        """
        super().__init__(x, y, width, height)
        self.font = font
        self.action = action

        button_h = height * 0.5
        label_h = height * 0.3
        margin = height * 0.1

        button_colors = colors.get("button", None) if colors else None
        self.train_button = Button(
            self.rect.left,
            self.rect.top,
            self.rect.width,
            button_h,
            self.font,
            button_text,
            button_colors,
            action=self._on_button_click,
        )

        label_y = self.train_button.rect.bottom + margin
        label_colors = colors.get("label", {}) if colors else {}
        self.status_label = Label(
            self.rect.left,
            label_y,
            self.rect.width,
            label_h,
            self.font,
            status_text,
            text_color=label_colors.get("text", (200, 200, 200)),
            bg_color=label_colors.get("bg", None),
        )
        self.training_active = False

    def _on_button_click(self):
        """
        Внутренний обработчик нажатия кнопки "Start/Stop Training".

        Изменяет состояние обучения и текст на кнопке/статусе.
        Вызывает внешний `action`, если он предоставлен.
        """
        if not self.training_active:
            self.set_status("Status: Training...")
            if self.action:
                self.action(True)

    def set_status(self, text):
        """
        Обновляет текст метки статуса.

        Args:
            text (str): Новый текст для метки статуса.
        """
        self.status_label.set_text(text)

    def handle_event(self, event):
        """
        Обрабатывает события для панели обучения.

        Передает события кнопке обучения.

        Args:
            event (pygame.event.Event): Событие Pygame.

        Returns:
            bool: True, если событие было обработано кнопкой, иначе False (если панель неактивна).
                  Если панель активна, но событие не для кнопки, возвращает True, если мышь над панелью.
        """
        if not self.visible:
            return False
        if not self.training_active:
            return self.train_button.handle_event(event)
        else:
            if event.type == pygame.MOUSEMOTION:
                self.train_button.handle_event(event)
            return self.rect.collidepoint(
                event.pos if hasattr(event, "pos") else (-1, -1)
            )

    def draw(self, surface):
        """
        Отрисовывает панель управления обучением на указанной поверхности.

        Включает кнопку и метку статуса.

        Args:
            surface (pygame.Surface): Поверхность Pygame для отрисовки.
        """
        if not self.visible:
            return
        self.train_button.draw(surface)
        self.status_label.draw(surface)
