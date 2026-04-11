import pytest
import pygame
from unittest.mock import MagicMock, call, ANY, patch
import math

from ai_life_simulator._front.front import *

# --- Константы для тестов ---
VIEWPORT_RECT = pygame.Rect(50, 50, 400, 300)
MAP_W = 20
MAP_H = 15
INITIAL_TILE_SIZE = 30
MIN_TILE_SIZE = 10
MAX_TILE_SIZE = 60
BROWN = (139, 69, 19)
GRAY = (128, 128, 128)
DARK_GRAY = (50, 50, 50)
YELLOW = (255, 255, 0)
SLIDER_TRACK_COLOR = (100, 100, 100)
SLIDER_HANDLE_COLOR = (210, 210, 210)


# region Test MapView
class TestMapView:
    """Тестирует класс MapView для отображения и взаимодействия с картой."""
    
    @pytest.fixture
    def map_view(self, mock_static_loader, mock_map_storage, dummy_surface):
        """Фикстура для создания экземпляра MapView с моками зависимостей."""
        view = MapView(
            VIEWPORT_RECT, MAP_W, MAP_H, mock_static_loader, mock_map_storage
        )
        # Настройка после инициализации
        view.min_tile_size = MIN_TILE_SIZE
        view.max_tile_size = MAX_TILE_SIZE
        view.tile_size = INITIAL_TILE_SIZE
        view._update_zoom_dependent_vars(center_view=True)
        return view

    def test_initialization(
        self, map_view, mock_static_loader, mock_map_storage, dummy_surface
    ):
        """Проверяет корректность инициализации MapView."""
        assert map_view.viewport_rect == VIEWPORT_RECT
        assert map_view.map_width_tiles == MAP_W
        assert map_view.map_height_tiles == MAP_H
        assert map_view.map_data == mock_map_storage
        assert map_view.mode == MapView.MODE_DRAG
        assert not map_view.paint_locked
        assert map_view.draw_color == BROWN

        eff_min = map_view._calculate_effective_min_tile_size()
        expected_initial_size = max(eff_min, min(INITIAL_TILE_SIZE, MAX_TILE_SIZE))
        assert map_view.tile_size == expected_initial_size

        assert isinstance(map_view.drag_button_rect, pygame.Rect)
        assert isinstance(map_view.paint_button_rect, pygame.Rect)
        assert isinstance(map_view.slider_track_rect, pygame.Rect)
        assert isinstance(map_view.slider_handle_rect, pygame.Rect)
        assert isinstance(map_view.drag_icon, MagicMock)
        assert isinstance(map_view.paint_icon, MagicMock)
        dummy_surface.mock_transform_smoothscale.assert_called()

        assert map_view.camera_x == pytest.approx(map_view.max_camera_x / 2)
        assert map_view.camera_y == pytest.approx(map_view.max_camera_y / 2)

        mock_static_loader.getLogger.assert_called_with(ANY)

    def test_calculate_effective_min_tile_size(self, map_view):
        """
        Проверяет расчет эффективного минимального размера тайла.

        Эффективный минимальный размер тайла зависит от размеров карты
        и области просмотра, чтобы карта всегда помещалась в область просмотра.
        """
        map_view.map_width_tiles = 100
        map_view.map_height_tiles = 100
        map_view.min_tile_size = 15
        map_view.viewport_rect = pygame.Rect(0, 0, 400, 300)
        assert map_view._calculate_effective_min_tile_size() == 15

        map_view.map_width_tiles = 10
        map_view.map_height_tiles = 50
        map_view.min_tile_size = 5
        map_view.viewport_rect = pygame.Rect(0, 0, 400, 300)
        assert map_view._calculate_effective_min_tile_size() == 40

        map_view.map_width_tiles = 50
        map_view.map_height_tiles = 5
        map_view.min_tile_size = 5
        map_view.viewport_rect = pygame.Rect(0, 0, 400, 300)
        assert map_view._calculate_effective_min_tile_size() == 60

        map_view.map_width_tiles = 0
        map_view.map_height_tiles = -5
        map_view.min_tile_size = 5
        map_view.viewport_rect = pygame.Rect(0, 0, 400, 300)
        assert map_view._calculate_effective_min_tile_size() == 400

    def test_update_zoom_dependent_vars_clamping(self, map_view):
        """
        Проверяет, что _update_zoom_dependent_vars корректно ограничивает tile_size.
        """
        map_view.min_tile_size = MIN_TILE_SIZE
        map_view.max_tile_size = MAX_TILE_SIZE
        eff_min = map_view._calculate_effective_min_tile_size()

        map_view.tile_size = 5
        map_view._update_zoom_dependent_vars()
        assert map_view.tile_size == eff_min

        map_view.tile_size = 100
        map_view._update_zoom_dependent_vars()
        assert map_view.tile_size == MAX_TILE_SIZE

        valid_size = (eff_min + MAX_TILE_SIZE) // 2
        map_view.tile_size = valid_size
        map_view._update_zoom_dependent_vars()
        assert map_view.tile_size == valid_size

    def test_get_tile_indices_at_pos(self, map_view, mock_pygame_mouse):
        """
        Проверяет получение индексов тайла по координатам мыши.
        """
        map_view.camera_x = 10
        map_view.camera_y = 20
        map_view.tile_size = 10

        # --- Случай 1: Попадание в тайл ---
        # map_x = 60-50+10 = 20; map_y = 70-50+20 = 40 -> row=4, col=2
        pos1 = (60, 70)
        mock_pygame_mouse["get_pos"].return_value = pos1
        # Вызываем реальный collidepoint, который должен вернуть True для viewport и False для UI
        assert map_view.get_tile_indices_at_pos(pos1) == (4, 2)

        # --- Случай 2: Вне карты ---
        # map_x = 215 -> col = 21 ( > MAP_W-1 = 19)
        pos_off_map = (VIEWPORT_RECT.left + MAP_W * 10 + 5, VIEWPORT_RECT.top + 5)
        mock_pygame_mouse["get_pos"].return_value = pos_off_map
        assert map_view.get_tile_indices_at_pos(pos_off_map) is None

        # --- Случай 3: Попадание на кнопку drag ---
        # Устанавливаем позицию мыши в центр кнопки
        drag_center = map_view.drag_button_rect.center
        mock_pygame_mouse["get_pos"].return_value = drag_center
        # Реальный collidepoint кнопки должен вернуть True
        assert map_view.get_tile_indices_at_pos(drag_center) is None

        # --- Случай 4: Снаружи viewport ---
        # Устанавливаем позицию вне viewport
        pos_outside = (VIEWPORT_RECT.left - 10, VIEWPORT_RECT.top - 10)
        mock_pygame_mouse["get_pos"].return_value = pos_outside
        # Реальный collidepoint viewport'а вернет False
        assert map_view.get_tile_indices_at_pos(pos_outside) is None

        # --- Случай 5: Нулевой размер тайла ---
        map_view.tile_size = 0
        mock_pygame_mouse["get_pos"].return_value = pos1
        assert map_view.get_tile_indices_at_pos(pos1) is None


    @pytest.mark.parametrize(
        "y, expected_val", [(15, 60), (165, 10), (90, 35), (0, 60), (200, 10)]
    )
    def test_y_to_value(self, map_view, y, expected_val):
        """
        Проверяет преобразование Y-координаты слайдера в значение размера тайла.
        """
        map_view.slider_track_rect = pygame.Rect(15, 15, 12, 150)
        map_view.max_tile_size = 60
        map_view.effective_min_tile_size = 10
        assert map_view._y_to_value(y) == expected_val

    @pytest.mark.parametrize(
        "value, expected_y", [(60, 15), (10, 165), (35, 90), (70, 15), (5, 165)]
    )
    def test_value_to_y(self, map_view, value, expected_y):
        """
        Проверяет преобразование значения размера тайла в Y-координату слайдера.
        """
        map_view.slider_track_rect = pygame.Rect(15, 15, 12, 150)
        map_view.max_tile_size = 60
        map_view.effective_min_tile_size = 10
        assert map_view._value_to_y(value) == expected_y

    def test_brush_mode_lock(self, map_view):
        """
        Проверяет блокировку и разблокировку режима рисования.
        """
        assert not map_view.paint_locked
        map_view.mode = MapView.MODE_PAINT
        map_view.ban_brush_mode()
        assert map_view.paint_locked is True
        assert map_view.mode == MapView.MODE_DRAG
        map_view.unlock_brush_mode()
        assert map_view.paint_locked is False
        assert map_view.mode == MapView.MODE_DRAG

    # --- Тесты Обработки Событий ---

    @pytest.mark.parametrize(
        "button_rect_attr, target_mode",
        [
            ("drag_button_rect", MapView.MODE_DRAG),
            ("paint_button_rect", MapView.MODE_PAINT),
        ],
    )
    def test_handle_event_mode_switch_buttons(
        self,
        map_view,
        mock_pygame_event,
        mock_pygame_mouse,
        button_rect_attr,
        target_mode,
    ):
        """
        Проверяет переключение режимов (DRAG/PAINT) через кнопки.
        """
        button_rect = getattr(map_view, button_rect_attr)
        initial_mode = (
            MapView.MODE_DRAG
            if target_mode == MapView.MODE_PAINT
            else MapView.MODE_PAINT
        )
        map_view.mode = initial_mode
        map_view.paint_locked = False

        click_pos = button_rect.center
        mock_pygame_mouse["get_pos"].return_value = click_pos  # Мышь над нужной кнопкой

        event = mock_pygame_event(pygame.MOUSEBUTTONUP, button=1, pos=click_pos)
        handled = map_view.handle_event(event)

        assert handled is True
        assert map_view.mode == target_mode

    def test_handle_event_paint_mode_locked_click(
        self, map_view, mock_pygame_event, mock_pygame_mouse
    ):
        """
        Проверяет переключение режимов (DRAG/PAINT) через кнопки.
        """
        map_view.ban_brush_mode()
        assert map_view.paint_locked
        assert map_view.mode == MapView.MODE_DRAG

        click_pos = map_view.paint_button_rect.center
        mock_pygame_mouse["get_pos"].return_value = click_pos  # Мышь над кнопкой paint

        event = mock_pygame_event(pygame.MOUSEBUTTONUP, button=1, pos=click_pos)
        handled = map_view.handle_event(event)

        assert handled is True
        assert map_view.mode == MapView.MODE_DRAG  # Режим не изменился

    def test_handle_event_slider_interaction(
        self, map_view, mock_pygame_event, mock_pygame_mouse, mocker
    ):
        """
        Проверяет взаимодействие со слайдером масштабирования.
        Включает клик по ползунку, перетаскивание ползунка и клик по треку.
        """
        mocker.spy(map_view, "_update_zoom_dependent_vars")
        mocker.spy(map_view, "_y_to_value")
        mocker.spy(map_view, "_value_to_y")

        initial_tile_size = map_view.tile_size
        handle_rect = map_view.slider_handle_rect
        track_rect = map_view.slider_track_rect

        # --- 1. Клик по ползунку ---
        handle_start_pos = handle_rect.center
        mock_pygame_mouse["get_pos"].return_value = handle_start_pos  # Мышь на ползунке
        event_down_handle = mock_pygame_event(
            pygame.MOUSEBUTTONDOWN, button=1, pos=handle_start_pos
        )
        handled = map_view.handle_event(event_down_handle)
        assert handled is True
        assert map_view.is_dragging_slider is True
        assert map_view.slider_handle_offset_y == pytest.approx(
            handle_start_pos[1] - handle_rect.top
        )
        map_view._update_zoom_dependent_vars.assert_not_called()

        # --- 2. Перетаскивание ползунка ---
        drag_pos_y = handle_start_pos[1] - 20
        drag_pos = (handle_start_pos[0], drag_pos_y)
        mock_pygame_mouse["get_pos"].return_value = drag_pos  # Мышь переместилась
        event_motion_handle = mock_pygame_event(pygame.MOUSEMOTION, pos=drag_pos)
        handled = map_view.handle_event(event_motion_handle)
        assert handled is True
        assert map_view.is_dragging_slider is True
        map_view._y_to_value.assert_called()
        assert handle_rect.centery == pytest.approx(
            map_view._value_to_y(map_view.tile_size)
        )
        if map_view.tile_size != initial_tile_size:
            map_view._update_zoom_dependent_vars.assert_called()
        else:
            map_view._update_zoom_dependent_vars.assert_not_called()

        # --- 3. Отпускание ползунка ---
        event_up_handle = mock_pygame_event(
            pygame.MOUSEBUTTONUP, button=1, pos=drag_pos
        )
        handled = map_view.handle_event(event_up_handle)
        assert handled is True
        assert map_view.is_dragging_slider is False
        map_view._update_zoom_dependent_vars.reset_mock()

        # --- 4. Клик по треку ---
        track_click_pos_y = track_rect.centery + 30
        track_click_pos = (track_rect.centerx, track_click_pos_y)
        mock_pygame_mouse["get_pos"].return_value = track_click_pos  # Мышь на треке
        event_down_track = mock_pygame_event(
            pygame.MOUSEBUTTONDOWN, button=1, pos=track_click_pos
        )

        size_before_track_click = map_view.tile_size
        handled = map_view.handle_event(event_down_track)
        assert handled is True
        assert map_view.is_dragging_slider is True
        map_view._y_to_value.assert_called_with(pytest.approx(track_click_pos_y))
        assert handle_rect.centery == pytest.approx(
            map_view._value_to_y(map_view.tile_size)
        )
        if map_view.tile_size != size_before_track_click:
            map_view._update_zoom_dependent_vars.assert_called()
        else:
            map_view._update_zoom_dependent_vars.assert_not_called()

        # --- 5. Отпускание после клика по треку ---
        event_up_track = mock_pygame_event(
            pygame.MOUSEBUTTONUP, button=1, pos=track_click_pos
        )
        handled = map_view.handle_event(event_up_track)
        assert handled is True
        assert map_view.is_dragging_slider is False

    def test_handle_event_map_drag(
        self, map_view, mock_pygame_event, mock_pygame_mouse, mocker
    ):
        """
        Проверяет перетаскивание карты (изменение положения камеры).
        """
        map_view.mode = MapView.MODE_DRAG
        mocker.spy(map_view, "_update_zoom_dependent_vars")

        start_cam_x, start_cam_y = map_view.camera_x, map_view.camera_y
        start_pos = (
            VIEWPORT_RECT.center
        )
        drag_pos = (start_pos[0] + 50, start_pos[1] - 30)

        # --- 1. Нажатие ---
        mock_pygame_mouse["get_pos"].return_value = start_pos
        event_down = mock_pygame_event(pygame.MOUSEBUTTONDOWN, button=1, pos=start_pos)
        handled = map_view.handle_event(event_down)
        assert handled is False
        assert map_view.dragging is True
        assert map_view.potential_click is False
        assert map_view.mouse_down_pos == start_pos
        assert map_view.start_drag_camera_pos == (start_cam_x, start_cam_y)

        # --- 2. Движение ---
        mock_pygame_mouse["get_pos"].return_value = drag_pos
        event_motion = mock_pygame_event(
            pygame.MOUSEMOTION, pos=drag_pos, buttons=(1, 0, 0)
        )
        handled = map_view.handle_event(event_motion)
        assert handled is False
        expected_cam_x = max(
            0, min(start_cam_x - (drag_pos[0] - start_pos[0]), map_view.max_camera_x)
        )
        expected_cam_y = max(
            0, min(start_cam_y - (drag_pos[1] - start_pos[1]), map_view.max_camera_y)
        )
        assert map_view.camera_x == pytest.approx(expected_cam_x)
        assert map_view.camera_y == pytest.approx(expected_cam_y)
        map_view._update_zoom_dependent_vars.assert_not_called()

        # --- 3. Отпускание ---
        event_up = mock_pygame_event(pygame.MOUSEBUTTONUP, button=1, pos=drag_pos)
        handled = map_view.handle_event(event_up)
        assert handled is False
        assert map_view.dragging is False
        assert map_view.mouse_down_pos is None
        assert map_view.start_drag_camera_pos is None

    def test_handle_event_map_paint(
        self, map_view, mock_pygame_event, mock_pygame_mouse, mocker
    ):
        """
        Проверяет рисование на карте в режиме PAINT.
        Включает одиночный клик и рисование с зажатой кнопкой мыши.
        """
        map_view.mode = MapView.MODE_PAINT
        map_view.paint_locked = False
        map_view.set_current_color((0, 0, 255))  # Blue
        target_color_code = map_view.decode.get((0, 0, 255), (None, "blue_code"))[0]
        start_pos = VIEWPORT_RECT.center  # Позиция внутри viewport и вне UI
        tile_pos_start = (5, 6)
        tile_pos_drag = (5, 7)

        # Мокаем get_tile_indices_at_pos
        def mock_get_indices(pos):
            dx = pos[0] - start_pos[0]
            dy = pos[1] - start_pos[1]
            dist_sq = dx * dx + dy * dy
            if dist_sq <= CLICK_MOVE_THRESHOLD * CLICK_MOVE_THRESHOLD:
                return tile_pos_start
            else:
                return tile_pos_drag

        mock_get_tile_indices = mocker.patch.object(
            map_view, "get_tile_indices_at_pos", side_effect=mock_get_indices
        )

        map_view.map_data[tile_pos_start[0]][tile_pos_start[1]] = "black_code"
        map_view.map_data[tile_pos_drag[0]][tile_pos_drag[1]] = "black_code"

        # --- 1. Нажатие ---
        mock_pygame_mouse["get_pos"].return_value = start_pos
        event_down = mock_pygame_event(pygame.MOUSEBUTTONDOWN, button=1, pos=start_pos)
        handled = map_view.handle_event(event_down)
        assert handled is False
        assert map_view.dragging is True
        assert map_view.potential_click is True
        mock_get_tile_indices.assert_called_with(start_pos)
        assert (
            map_view.map_data[tile_pos_start[0]][tile_pos_start[1]] == target_color_code
        )

        # --- 2. Движение (короткое) ---
        motion_pos_short = (start_pos[0] + 1, start_pos[1])
        mock_pygame_mouse["get_pos"].return_value = motion_pos_short
        event_motion_short = mock_pygame_event(
            pygame.MOUSEMOTION, pos=motion_pos_short, buttons=(1, 0, 0)
        )
        handled = map_view.handle_event(event_motion_short)
        assert handled is False
        assert map_view.potential_click is True
        assert map_view.map_data[tile_pos_drag[0]][tile_pos_drag[1]] == "black_code"

        # --- 3. Движение (длинное) ---
        motion_pos_long = (start_pos[0] + CLICK_MOVE_THRESHOLD * 2, start_pos[1])
        mock_pygame_mouse["get_pos"].return_value = motion_pos_long
        event_motion_long = mock_pygame_event(
            pygame.MOUSEMOTION, pos=motion_pos_long, buttons=(1, 0, 0)
        )
        handled = map_view.handle_event(event_motion_long)
        assert handled is False
        assert map_view.potential_click is False
        mock_get_tile_indices.assert_called_with(motion_pos_long)
        assert (
            map_view.map_data[tile_pos_drag[0]][tile_pos_drag[1]] == target_color_code
        )

        # --- 4. Отпускание ---
        event_up = mock_pygame_event(
            pygame.MOUSEBUTTONUP, button=1, pos=motion_pos_long
        )
        handled = map_view.handle_event(event_up)
        assert handled is False
        assert map_view.dragging is False
        assert map_view.potential_click is False

    def test_handle_event_zoom_wheel(self, map_view, mock_pygame_event, mocker):
        """
        Проверяет масштабирование карты с помощью колеса мыши.
        """
        mocker.spy(map_view, "_update_zoom_dependent_vars")
        initial_tile_size = map_view.tile_size
        map_view.min_tile_size = MIN_TILE_SIZE
        map_view.max_tile_size = MAX_TILE_SIZE

        # Zoom In
        event_zoom_in = mock_pygame_event(
            pygame.MOUSEWHEEL, y=1, pos=VIEWPORT_RECT.center
        )
        handled = map_view.handle_event(event_zoom_in)
        assert handled is True
        expected_size_in_base = int(round(initial_tile_size * 1.1))
        expected_size_in_base = max(
            MIN_TILE_SIZE, min(expected_size_in_base, MAX_TILE_SIZE)
        )
        map_view._update_zoom_dependent_vars.assert_called_once()
        eff_min_after_in = map_view._calculate_effective_min_tile_size()
        assert map_view.tile_size == max(eff_min_after_in, expected_size_in_base)

        # Zoom Out
        map_view._update_zoom_dependent_vars.reset_mock()
        current_tile_size = map_view.tile_size
        event_zoom_out = mock_pygame_event(
            pygame.MOUSEWHEEL, y=-1, pos=VIEWPORT_RECT.center
        )
        handled = map_view.handle_event(event_zoom_out)
        assert handled is True
        expected_size_out_base = int(round(current_tile_size / 1.1))
        expected_size_out_base = max(
            MIN_TILE_SIZE, min(expected_size_out_base, MAX_TILE_SIZE)
        )
        map_view._update_zoom_dependent_vars.assert_called_once()
        eff_min_after_out = map_view._calculate_effective_min_tile_size()
        assert map_view.tile_size == max(eff_min_after_out, expected_size_out_base)

    def test_set_current_color(self, map_view):
        """Проверяет установку текущего цвета для рисования."""
        assert map_view.draw_color == BROWN
        new_color = (0, 255, 0)
        map_view.set_current_color(new_color)
        assert map_view.draw_color == new_color

    def test_draw(self, map_view, dummy_surface):
        """
        Проверяет отрисовку MapView, включая тайлы, сетку и UI элементы.
        """
        mock_draw_rect = dummy_surface.mock_draw_rect
        mock_blit = dummy_surface.blit
        dummy_surface.reset_draw_mocks()

        map_view.mode = MapView.MODE_PAINT
        map_view.tile_size = 20
        map_view.camera_x = 5
        map_view.camera_y = 5
        map_view._update_zoom_dependent_vars()

        map_view.draw(dummy_surface)

        # 1. Clipping
        dummy_surface.set_clip.assert_has_calls(
            [call(map_view.viewport_rect), call(ANY)], any_order=False
        )

        # 2. Tile Drawing
        tile_fill_calls = [
            c
            for c in mock_draw_rect.call_args_list
            if len(c.args) == 3 and isinstance(c.args[2], pygame.Rect)
        ]
        assert len(tile_fill_calls) > 0, "No tile fill rects drawn"

        # 3. Grid Drawing
        grid_calls = [
            c
            for c in mock_draw_rect.call_args_list
            if len(c.args) == 4 and c.args[3] == 1
        ]
        if map_view.tile_size > 5:
            assert len(grid_calls) > 0
        else:
            assert len(grid_calls) == 0

        # 4. UI Drawing
        mock_draw_rect.assert_any_call(
            dummy_surface, ANY, map_view.drag_button_rect, border_radius=ANY
        )
        mock_draw_rect.assert_any_call(
            dummy_surface, ANY, map_view.paint_button_rect, border_radius=ANY
        )

        assert any(
            c.args[1:3] == (DARK_GRAY, map_view.paint_button_rect)
            and c.kwargs.get("width", 0) > 0
            for c in mock_draw_rect.call_args_list
        )

        mock_blit.assert_any_call(map_view.drag_icon, ANY)
        mock_blit.assert_any_call(map_view.paint_icon, ANY)

        slider_track_call_found = False
        for args, kwargs in mock_draw_rect.call_args_list:
            if (
                len(args) >= 3
                and args[0] is dummy_surface
                and args[1] == SLIDER_TRACK_COLOR
                and args[2] is map_view.slider_track_rect
            ):
                if kwargs.get("border_radius") == map_view.slider_border_radius:
                    slider_track_call_found = True
                    break
        assert slider_track_call_found, f"Slider track rect not drawn correctly"

        mock_draw_rect.assert_any_call(
            dummy_surface,
            SLIDER_HANDLE_COLOR,
            map_view.slider_handle_rect,
            border_radius=ANY,
        )
        assert any(
            c.args[1:3] == (DARK_GRAY, map_view.slider_handle_rect)
            and c.kwargs.get("width", 0) > 0
            for c in mock_draw_rect.call_args_list
        )


# endregion


# region Test UIElement
class TestUIElement:
    """Тестирует базовый класс UIElement."""
    
    @pytest.fixture
    def ui_element(self, mock_static_loader):
        """Фикстура для создания экземпляра UIElement."""
        return UIElement(10, 10, 100, 50)

    def test_initialization(self, ui_element):
        """Проверяет инициализацию UIElement."""
        assert ui_element.rect == pygame.Rect(10, 10, 100, 50)
        assert ui_element.visible is True
        assert ui_element.hovered is False
        assert isinstance(ui_element.logger, MagicMock)

    def test_handle_event_visibility(self, ui_element, mock_pygame_event):
        """Проверяет, что невидимый элемент не обрабатывает события и не меняет состояние hovered."""
        ui_element.visible = False
        event = mock_pygame_event(pygame.MOUSEMOTION, pos=(20, 20))
        assert ui_element.handle_event(event) is False
        assert ui_element.hovered is False

    def test_handle_event_hover(self, ui_element, mock_pygame_event, mock_pygame_mouse):
        """Проверяет изменение состояния hovered при наведении мыши."""
        ui_element.visible = True
        hover_pos = ui_element.rect.center
        mock_pygame_mouse["get_pos"].return_value = hover_pos
        event_on = mock_pygame_event(pygame.MOUSEMOTION, pos=hover_pos)
        ui_element.handle_event(event_on)
        assert ui_element.hovered is True
        off_pos = (ui_element.rect.right + 10, ui_element.rect.top)
        mock_pygame_mouse["get_pos"].return_value = off_pos
        event_off = mock_pygame_event(pygame.MOUSEMOTION, pos=off_pos)
        ui_element.handle_event(event_off)
        assert ui_element.hovered is False

    def test_handle_event_click_delegation(self, ui_element, mock_pygame_event, mocker):
        """Проверяет, что событие клика делегируется методу on_click, если элемент hovered."""
        ui_element.visible = True
        ui_element.hovered = True
        mock_on_click = mocker.patch.object(
            ui_element, "on_click", return_value="result_from_onclick"
        )
        event_down = mock_pygame_event(
            pygame.MOUSEBUTTONDOWN, button=1, pos=ui_element.rect.center
        )
        result = ui_element.handle_event(event_down)
        mock_on_click.assert_called_once_with(event_down)
        assert result == "result_from_onclick"

    def test_on_click(self, ui_element, mock_pygame_event):
        """Проверяет базовую реализацию on_click (должна возвращать True)."""
        event = mock_pygame_event(
            pygame.MOUSEBUTTONDOWN, button=1, pos=ui_element.rect.center
        )
        assert ui_element.on_click(event) is True

    def test_draw_visibility(self, ui_element, dummy_surface):
        """Проверяет, что невидимый элемент не отрисовывается."""
        ui_element.visible = False
        dummy_surface.reset_draw_mocks()
        ui_element.draw(dummy_surface)
        assert dummy_surface.mock_draw_rect.call_count == 0
        assert dummy_surface.blit.call_count == 0


# endregion


# region Test Button
class TestButton:
    """Тестирует класс Button."""
    
    @pytest.fixture
    def mock_action(self):
        """Фикстура для мока действия кнопки."""
        return MagicMock(name="ButtonAction")

    @pytest.fixture
    def button(
        self, mock_pygame_font, mock_action, mock_static_loader
    ):
        """Фикстура для создания экземпляра Button."""
        colors = {"normal": (0, 0, 100), "hover": (0, 0, 150), "active": (0, 0, 50)}
        return Button(
            20,
            30,
            120,
            40,
            mock_pygame_font,
            "Click Me",
            colors,
            mock_action,
            border_radius=3,
        )

    def test_initialization(self, button, mock_pygame_font):
        """Проверяет инициализацию кнопки."""
        assert button.text == "Click Me"
        assert button.font == mock_pygame_font
        assert button.action is not None
        assert button.current_color == button.colors["normal"]
        assert not button.is_active
        mock_pygame_font.render.assert_called_with("Click Me", True, button.text_color)
        assert isinstance(button.text_surface, MagicMock)
        assert isinstance(button.text_rect, pygame.Rect)

    def test_handle_event_hover_colors(
        self, button, mock_pygame_event, mock_pygame_mouse
    ):
        """Проверяет изменение цвета кнопки при наведении и убирании мыши."""
        hover_pos = button.rect.center
        mock_pygame_mouse["get_pos"].return_value = hover_pos
        event_motion_on = mock_pygame_event(pygame.MOUSEMOTION, pos=hover_pos)
        button.handle_event(event_motion_on)
        assert button.hovered is True
        assert button.current_color == button.colors["hover"]
        off_pos = (0, 0)
        mock_pygame_mouse["get_pos"].return_value = off_pos
        event_motion_off = mock_pygame_event(pygame.MOUSEMOTION, pos=off_pos)
        button.handle_event(event_motion_off)
        assert button.hovered is False
        assert button.current_color == button.colors["normal"]

    def test_handle_event_click_cycle(
        self, button, mock_pygame_event, mock_pygame_mouse, mock_action
    ):
        """Проверяет полный цикл клика: MOUSEBUTTONDOWN и MOUSEBUTTONUP на кнопке."""
        click_pos = button.rect.center
        mock_pygame_mouse["get_pos"].return_value = click_pos
        event_down = mock_pygame_event(pygame.MOUSEBUTTONDOWN, button=1, pos=click_pos)
        handled = button.handle_event(event_down)
        assert handled is True
        assert button.is_active is True
        assert button.current_color == button.colors["active"]
        mock_action.assert_not_called()
        event_up = mock_pygame_event(pygame.MOUSEBUTTONUP, button=1, pos=click_pos)
        handled = button.handle_event(event_up)
        assert handled is True
        assert button.is_active is False
        assert button.hovered is True
        assert button.current_color == button.colors["hover"]
        mock_action.assert_called_once()

    def test_handle_event_click_and_drag_off(
        self, button, mock_pygame_event, mock_pygame_mouse, mock_action
    ):
        """
        Проверяет, что действие не выполняется, если мышь нажата на кнопке,
        а затем отпущена вне ее пределов.
        """
        click_pos = button.rect.center
        drag_off_pos = (0, 0)
        mock_pygame_mouse["get_pos"].return_value = click_pos
        event_down = mock_pygame_event(pygame.MOUSEBUTTONDOWN, button=1, pos=click_pos)
        button.handle_event(event_down)
        move_pos_on = (click_pos[0] + 1, click_pos[1])
        mock_pygame_mouse["get_pos"].return_value = move_pos_on
        event_motion_on = mock_pygame_event(
            pygame.MOUSEMOTION, pos=move_pos_on, buttons=(1, 0, 0)
        )
        button.handle_event(event_motion_on)
        mock_pygame_mouse["get_pos"].return_value = drag_off_pos
        event_motion_off = mock_pygame_event(
            pygame.MOUSEMOTION, pos=drag_off_pos, buttons=(1, 0, 0)
        )
        button.handle_event(event_motion_off)
        event_up_off = mock_pygame_event(
            pygame.MOUSEBUTTONUP, button=1, pos=drag_off_pos
        )
        handled = button.handle_event(event_up_off)
        assert handled is True
        assert button.is_active is False
        assert button.hovered is False
        assert button.current_color == button.colors["normal"]
        mock_action.assert_not_called()

    def test_draw(self, button, dummy_surface):
        """Проверяет отрисовку кнопки в обычном и активном (нажатом) состоянии."""
        mock_draw_rect = dummy_surface.mock_draw_rect
        mock_blit = dummy_surface.blit
        dummy_surface.reset_draw_mocks()
        button.is_active = False
        button.current_color = button.colors["normal"]
        button.draw(dummy_surface)
        mock_draw_rect.assert_called_with(
            dummy_surface,
            button.colors["normal"],
            button.rect,
            border_radius=button.border_radius,
        )
        mock_blit.assert_called_with(
            button.text_surface, (button.text_rect.x, button.text_rect.y)
        )
        dummy_surface.reset_draw_mocks()
        button.is_active = True
        button.current_color = button.colors["active"]
        button.draw(dummy_surface)
        mock_draw_rect.assert_called_with(
            dummy_surface,
            button.colors["active"],
            button.rect,
            border_radius=button.border_radius,
        )
        expected_text_pos = (
            button.text_rect.x + button.click_offset,
            button.text_rect.y + button.click_offset,
        )
        mock_blit.assert_called_with(button.text_surface, expected_text_pos)

    def test_set_text(self, button, mock_pygame_font):
        """Проверяет изменение текста кнопки и перерисовку текстовой поверхности."""
        mock_pygame_font.render.reset_mock()
        button.set_text("New Text")
        assert button.text == "New Text"
        mock_pygame_font.render.assert_called_once_with(
            "New Text", True, button.text_color
        )
        assert isinstance(button.text_surface, MagicMock)


# endregion


# region Test PaletteItem
class TestPaletteItem:
    """Тестирует класс PaletteItem, представляющий элемент палитры."""
    
    @pytest.fixture
    def palette_item(self, mock_static_loader):
        """Фикстура для создания экземпляра PaletteItem."""
        return PaletteItem(
            x=5,
            y=5,
            size=20,
            item_type="type_red",
            color=(255, 0, 0),
            icon=None,
            selected_border_color=YELLOW,
            border_width=3,
        )

    def test_initialization(self, palette_item):
        """Проверяет инициализацию элемента палитры."""
        assert palette_item.rect == pygame.Rect(5, 5, 20, 20)
        assert palette_item.item_type == "type_red"
        assert palette_item.color == (255, 0, 0)
        assert palette_item.icon is None
        assert palette_item.selected is False

    def test_on_click(self, palette_item, mock_pygame_event):
        """Проверяет, что on_click возвращает True (базовое поведение UIElement)."""
        event = mock_pygame_event(pygame.MOUSEBUTTONDOWN, button=1)
        assert palette_item.on_click(event) is True

    def test_draw(self, palette_item, dummy_surface, mocker):
        """Проверяет отрисовку элемента палитры в разных состояниях (обычный, выбранный, наведенный)."""
        mock_draw_rect = dummy_surface.mock_draw_rect
        mock_blit = dummy_surface.blit
        mock_surface_class = mocker.patch(
            "pygame.Surface",
            return_value=MagicMock(spec=pygame.Surface, name="HoverSurface"),
        )
        dummy_surface.reset_draw_mocks()
        mock_blit.reset_mock()
        mock_surface_class.reset_mock()

        # Normal
        palette_item.selected = False
        palette_item.hovered = False
        palette_item.draw(dummy_surface)
        mock_draw_rect.assert_called_with(
            dummy_surface, palette_item.color, palette_item.rect
        )
        assert not any(
            c.args[1] == palette_item.selected_border_color
            for c in mock_draw_rect.call_args_list
        )
        mock_blit.assert_not_called()
        dummy_surface.reset_draw_mocks()

        # Selected
        palette_item.selected = True
        palette_item.hovered = False
        palette_item.draw(dummy_surface)
        mock_draw_rect.assert_any_call(
            dummy_surface, palette_item.color, palette_item.rect
        )
        mock_draw_rect.assert_any_call(
            dummy_surface,
            palette_item.selected_border_color,
            palette_item.rect,
            palette_item.border_width,
        )
        mock_blit.assert_not_called()
        dummy_surface.reset_draw_mocks()

        # Hovered
        palette_item.selected = False
        palette_item.hovered = True
        palette_item.draw(dummy_surface)
        mock_draw_rect.assert_any_call(
            dummy_surface, palette_item.color, palette_item.rect
        )
        assert not any(
            c.args[1] == palette_item.selected_border_color
            for c in mock_draw_rect.call_args_list
        )
        mock_surface_class.assert_called_once_with(
            palette_item.rect.size, pygame.SRCALPHA
        )
        hover_surface_mock = mock_surface_class.return_value
        hover_surface_mock.fill.assert_called_once_with((255, 255, 255, 50))
        mock_blit.assert_called_once_with(hover_surface_mock, palette_item.rect.topleft)


# endregion


# region Test Palette
class TestPalette:
    """Тестирует класс Palette, управляющий набором PaletteItem."""
    
    @pytest.fixture
    def items_data(self):
        """Данные для элементов палитры."""
        return [
            {"type": "red_code", "color": (255, 0, 0)},
            {"type": "green_code", "color": (0, 255, 0)},
            {"type": "blue_code", "color": (0, 0, 255)},
            {"type": "black_code", "color": (0, 0, 0)},
        ]

    @pytest.fixture
    def mock_on_select(self):
        """Мок для колбэка on_select палитры."""
        return MagicMock(name="PaletteOnSelect")

    @pytest.fixture
    def palette(self, items_data, mock_on_select, mock_static_loader):
        """Фикстура для создания экземпляра Palette."""
        return Palette(
            10,
            10,
            100,
            100,
            items_data,
            cols=2,
            item_size=25,
            margin=5,
            on_select=mock_on_select,
        )

    def test_initialization(self, palette, items_data):
        """Проверяет инициализацию палитры и создание элементов."""
        assert len(palette.items) == len(items_data)
        assert isinstance(palette.items[0], PaletteItem)
        assert palette.selected_item_type is None
        assert palette.items[0].rect.topleft == (10, 10)
        assert palette.items[1].rect.topleft == (10 + 25 + 5, 10)
        assert palette.items[2].rect.topleft == (10, 10 + 25 + 5)

    def test_handle_event_item_click(
        self, palette, mock_pygame_event, mock_pygame_mouse, mock_on_select, mocker
    ):
        """Проверяет обработку клика по элементу палитры и вызов on_select."""
        target_item_index = 1
        target_item = palette.items[target_item_index]
        click_pos = target_item.rect.center
        mock_pygame_mouse["get_pos"].return_value = click_pos
        for i, item in enumerate(palette.items):
            mocker.patch.object(
                item, "handle_event", return_value=(i == target_item_index)
            )
        event_down = mock_pygame_event(pygame.MOUSEBUTTONDOWN, button=1, pos=click_pos)
        handled = palette.handle_event(event_down)
        assert handled is True
        assert palette.selected_item_type == target_item.item_type
        assert target_item.selected is True
        assert palette.items[0].selected is False
        mock_on_select.assert_called_once_with(target_item.item_type)

    def test_handle_event_motion_hover(
        self, palette, mock_pygame_event, mock_pygame_mouse, mocker
    ):
        """Проверяет, что события MOUSEMOTION передаются всем элементам палитры."""
        
        item0 = palette.items[0]
        item1 = palette.items[1]
        pos_over_item0 = item0.rect.center
        mock_pygame_mouse["get_pos"].return_value = pos_over_item0
        spy0 = mocker.spy(item0, "handle_event")
        spy1 = mocker.spy(item1, "handle_event")
        event_motion = mock_pygame_event(pygame.MOUSEMOTION, pos=pos_over_item0)
        handled = palette.handle_event(event_motion)
        assert spy0.call_count > 0
        assert spy1.call_count > 0
        assert handled is True

    def test_select_item(self, palette, mock_on_select):
        """Проверяет программный выбор элемента палитры."""
        item_type_to_select = palette.items[2].item_type

        palette.select_item(item_type_to_select)
        assert palette.selected_item_type == item_type_to_select
        assert palette.items[2].selected is True
        mock_on_select.assert_called_once_with(item_type_to_select)

        mock_on_select.reset_mock()
        palette.select_item(item_type_to_select)
        assert palette.selected_item_type == item_type_to_select  # State remains
        mock_on_select.assert_not_called()

        mock_on_select.reset_mock()
        new_item_type = palette.items[0].item_type
        palette.select_item(new_item_type)
        assert palette.selected_item_type == new_item_type
        assert palette.items[0].selected is True
        assert palette.items[2].selected is False
        mock_on_select.assert_called_once_with(new_item_type)

    def test_draw(self, palette, dummy_surface, mocker):
        """Проверяет, что палитра вызывает draw для всех своих элементов."""
        mock_item_draw = mocker.patch.object(PaletteItem, "draw")
        palette.draw(dummy_surface)
        assert mock_item_draw.call_count == len(palette.items)
        mock_item_draw.assert_called_with(dummy_surface)


# endregion


# region Test Switch
class TestSwitch:
    """Тестирует класс Switch (переключатель)."""

    @pytest.fixture
    def mock_switch_action(self):
        """Мок для действия переключателя."""
        return MagicMock(name="SwitchAction")

    @pytest.fixture
    def switch(
        self, mock_pygame_font, mock_switch_action, mock_static_loader
    ):
        """Фикстура для создания экземпляра Switch."""
        return Switch(
            50,
            50,
            60,
            25,
            mock_pygame_font,
            labels=("L", "R"),
            action=mock_switch_action,
            initial_state=False,
        )

    def test_initialization(self, switch):
        """Проверяет инициализацию переключателя."""
        assert switch.state is False
        assert switch.labels == ("L", "R")
        assert switch.current_knob_color == switch.colors["knob"]
        assert switch.label_font is not None

    def test_on_click_toggle(self, switch, mock_pygame_event, mock_switch_action):
        """Проверяет переключение состояния и вызов действия при клике."""
        event = mock_pygame_event(pygame.MOUSEBUTTONDOWN, button=1)
        result = switch.on_click(event)
        assert result is True
        assert switch.state is True
        mock_switch_action.assert_called_once_with("R")
        mock_switch_action.reset_mock()
        result = switch.on_click(event)
        assert result is True
        assert switch.state is False
        mock_switch_action.assert_called_once_with("L")

    def test_handle_event_hover_color(
        self, switch, mock_pygame_event, mock_pygame_mouse
    ):
        """Проверяет изменение цвета ручки переключателя при наведении мыши."""
        hover_pos = switch.rect.center
        mock_pygame_mouse["get_pos"].return_value = hover_pos
        event_motion_on = mock_pygame_event(pygame.MOUSEMOTION, pos=hover_pos)
        switch.handle_event(event_motion_on)
        assert switch.hovered is True
        assert switch.current_knob_color == switch.colors["knob_hover"]
        off_pos = (0, 0)
        mock_pygame_mouse["get_pos"].return_value = off_pos
        event_motion_off = mock_pygame_event(pygame.MOUSEMOTION, pos=off_pos)
        switch.handle_event(event_motion_off)
        assert switch.hovered is False
        assert switch.current_knob_color == switch.colors["knob"]

    def test_draw(self, switch, dummy_surface, mock_pygame_font):
        """Проверяет отрисовку переключателя в состояниях ON и OFF, с учетом наведения."""
        mock_draw_rect = dummy_surface.mock_draw_rect
        mock_draw_circle = dummy_surface.mock_draw_circle
        mock_blit = dummy_surface.blit
        dummy_surface.reset_draw_mocks()
        mock_blit.reset_mock()
        mock_pygame_font.render.reset_mock()

        # State OFF
        switch.state = False
        switch.hovered = False
        switch.current_knob_color = switch.colors["knob"]
        switch.draw(dummy_surface)
        mock_draw_rect.assert_called_with(
            dummy_surface, switch.colors["bg_off"], switch.rect, border_radius=ANY
        )
        mock_draw_circle.assert_called_with(
            dummy_surface,
            switch.colors["knob"],
            (switch.knob_x_off, switch.rect.centery),
            switch.knob_radius,
        )
        mock_pygame_font.render.assert_called_once_with("L", True, switch.label_color)
        mock_blit.assert_called_once_with(mock_pygame_font.render.return_value, ANY)
        dummy_surface.reset_draw_mocks()
        mock_blit.reset_mock()
        mock_pygame_font.render.reset_mock()

        # State ON, Hovered
        switch.state = True
        switch.hovered = True
        switch.current_knob_color = switch.colors["knob_hover"]
        switch.draw(dummy_surface)
        mock_draw_rect.assert_called_with(
            dummy_surface, switch.colors["bg_on"], switch.rect, border_radius=ANY
        )
        mock_draw_circle.assert_called_with(
            dummy_surface,
            switch.colors["knob_hover"],
            (switch.knob_x_on, switch.rect.centery),
            switch.knob_radius,
        )
        mock_pygame_font.render.assert_called_once_with("R", True, switch.label_color)
        mock_blit.assert_called_once_with(mock_pygame_font.render.return_value, ANY)


# endregion


# region Test Label
class TestLabel:
    """Тестирует класс Label для отображения текста."""

    @pytest.fixture
    def label(self, mock_pygame_font):
        """Фикстура для создания экземпляра Label."""
        return Label(
            100,
            100,
            150,
            30,
            mock_pygame_font,
            "Initial Text",
            alignment="left",
            bg_color=(10, 10, 10),
        )

    def test_initialization(
        self, label, mock_pygame_font
    ):
        """Проверяет инициализацию метки."""
        assert label.text == "Initial Text"
        assert label.font == mock_pygame_font
        mock_pygame_font.render.assert_called_with(
            "Initial Text", True, label.text_color
        )

    def test_set_text(self, label):
        """Проверяет установку нового текста и флага text_is_set."""
        assert label.text_is_set is False
        label.set_text("New Label Text")
        assert label.text == "New Label Text"
        assert label.text_is_set is True
        label.text_is_set = False
        label.set_text("New Label Text")
        assert label.text_is_set is False

    def test_handle_event(self, label, mock_pygame_event):
        """Проверяет, что Label не обрабатывает события (возвращает False)."""
        event = mock_pygame_event(pygame.MOUSEBUTTONDOWN)
        assert label.handle_event(event) is False

    def test_draw_and_set_text(
        self, label, dummy_surface, mock_pygame_font
    ):
        """
        Проверяет отрисовку метки и эффект set_text на отрисовку.
        В частности, фон должен рисоваться только если text_is_set (т.е. текст изменился).
        """
        mock_draw_rect = dummy_surface.mock_draw_rect
        mock_blit = dummy_surface.blit
        dummy_surface.reset_draw_mocks()
        mock_blit.reset_mock()
        mock_pygame_font.render.reset_mock()

        label.draw(dummy_surface)
        mock_pygame_font.render.assert_not_called()
        mock_draw_rect.assert_not_called()
        mock_blit.assert_called_once_with(label.text_surface, label.text_rect)
        dummy_surface.reset_draw_mocks()
        mock_blit.reset_mock()

        label.set_text("Updated Text")
        label.draw(dummy_surface)
        mock_pygame_font.render.assert_called_once_with(
            "Updated Text", True, label.text_color
        )
        assert label.text_is_set is False
        mock_draw_rect.assert_called_once_with(
            dummy_surface, label.bg_color, label.rect
        )
        mock_blit.assert_called_once_with(label.text_surface, label.text_rect)
# endregion


# region Test TrainingPanel
class TestTrainingPanel:
    """Тестирует TrainingPanel, содержащую кнопку и метку статуса."""

    @pytest.fixture
    def mock_panel_action(self):
        """Мок для действия, вызываемого панелью."""
        return MagicMock(name="PanelAction")

    @pytest.fixture
    def training_panel(self, mock_pygame_font, mock_panel_action, mock_static_loader):
        """Фикстура для создания экземпляра TrainingPanel."""
        return TrainingPanel(
            200, 200, 150, 60, mock_pygame_font, action=mock_panel_action
        )

    def test_initialization(
        self, training_panel, mock_pygame_font
    ):
        """Проверяет инициализацию панели, кнопки и метки."""
        assert isinstance(training_panel.train_button, Button)
        assert isinstance(training_panel.status_label, Label)
        mock_pygame_font.render.assert_any_call("Start Training", True, ANY)
        mock_pygame_font.render.assert_any_call("Status: Idle", True, ANY)

    def test_button_click_action(
        self, training_panel, mock_panel_action, mocker
    ):
        """Проверяет, что внутренний _on_button_click вызывает set_status и внешнее действие."""
        mock_set_status = mocker.patch.object(training_panel, "set_status")
        training_panel._on_button_click()
        mock_set_status.assert_called_once_with("Status: Training...")
        mock_panel_action.assert_called_once_with(True)

    def test_set_status(self, training_panel, mocker):
        """Проверяет, что set_status обновляет текст метки."""
        mock_label_set_text = mocker.spy(training_panel.status_label, "set_text")
        training_panel.set_status("New Status Update")
        mock_label_set_text.assert_called_once_with("New Status Update")

    def test_handle_event_delegation(
        self, training_panel, mock_pygame_event, mock_pygame_mouse, mocker
    ):
        """
        Проверяет делегирование событий кнопке, и блокировку кнопки во время 'training_active'.
        """
        mock_button_handle = mocker.patch.object(
            training_panel.train_button,
            "handle_event",
            return_value="handled_by_button",
        )
        event_down_pos_outside = (-10, -10)  # Позиция вне панели
        event_down_pos_inside = training_panel.rect.center  # Позиция внутри панели

        # --- Тренинг НЕ активен ---
        training_panel.training_active = False
        # Используем любую позицию, т.к. делегирование не зависит от коллизии с панелью
        mock_pygame_mouse["get_pos"].return_value = event_down_pos_inside
        event_down = mock_pygame_event(
            pygame.MOUSEBUTTONDOWN, pos=event_down_pos_inside
        )
        handled = training_panel.handle_event(event_down)
        mock_button_handle.assert_called_once_with(event_down)
        assert handled == "handled_by_button"
        mock_button_handle.reset_mock()

        # --- Тренинг АКТИВЕН ---
        training_panel.training_active = True

        # Клик ВНУТРИ панели
        mock_pygame_mouse["get_pos"].return_value = event_down_pos_inside
        event_down_in = mock_pygame_event(
            pygame.MOUSEBUTTONDOWN, pos=event_down_pos_inside
        )
        handled = training_panel.handle_event(event_down_in)
        mock_button_handle.assert_not_called()  # Клик НЕ передается
        # Возвращает True, т.к. реальный collidepoint панели вернет True
        assert handled is True

        # Клик ВНЕ панели
        mock_pygame_mouse["get_pos"].return_value = event_down_pos_outside
        event_down_out = mock_pygame_event(
            pygame.MOUSEBUTTONDOWN, pos=event_down_pos_outside
        )
        handled = training_panel.handle_event(event_down_out)
        mock_button_handle.assert_not_called()
        assert handled is False

        # Движение мыши (ВНУТРИ панели)
        mock_pygame_mouse["get_pos"].return_value = event_down_pos_inside
        event_motion = mock_pygame_event(pygame.MOUSEMOTION, pos=event_down_pos_inside)
        handled = training_panel.handle_event(event_motion)
        mock_button_handle.assert_called_once_with(event_motion)
        assert handled is True

    def test_draw_delegation(self, training_panel, dummy_surface, mocker):
        """Проверяет, что отрисовка панели делегируется кнопке и метке."""
        mock_button_draw = mocker.patch.object(training_panel.train_button, "draw")
        mock_label_draw = mocker.patch.object(training_panel.status_label, "draw")
        training_panel.draw(dummy_surface)
        mock_button_draw.assert_called_once_with(dummy_surface)
        mock_label_draw.assert_called_once_with(dummy_surface)


# endregion
