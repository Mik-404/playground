from ai_life_simulator._front.game_logic import *
import pygame
import sys

config = {
    'mode' : pygame.NOFRAME,
    'fps' : 30,
    'game_zone_size' : 0.65,
    'field_size_in_per' : 0.97,
    'standard_tile_height' : 50,
    'standard_tile_width' : 60,
    'menu_back_height' : 0.85,
    'TitleMarginTopPer': 0.05,
    'CloseButtonSizePer' : 0.4,
    'CloseButtonMargin' : 0.2,

    'ControlPanelMarginXPer': 0.03,


    'PaletteCols': 3,
    'SwitchHeightPer': 0.15,
    'SwitchWidthPer': 0.16,

    'UpperPanelHeightPer': 0.5,
    'ButtonHeightPer': 0.23,
    'ControlPanelWidthPer': 0.4,
    'PaletteItemSizePer': 0.14,


    'BottomPanelHeightPer': 0.25,


    'PanelMarginPer': 0.06,
    'ButtonMarginYPer': 0.03,
    'PaletteMarginPer': 0.02,
    'ComputeMargin' : 0.12,
    'TraningNegativeMargin' : 0.12
}


def main ():
    """
    Основная функция игры, управляющая инициализацией и выполнением игрового цикла.

    Выполняет:
    - Инициализацию игрового движка
    - Настройку логгера
    - Обработку событий ввода
    - Отрисовку игровых элементов
    - Управление частотой обновления кадров

    Побочные эффекты:
    - Создает графическое окно Pygame
    - Загружает системные ресурсы
    - Возможно, использует графичекий ускоритель
    - Завершает работу приложения при выходе

    Завершение работы:
    - По крестику окна
    - Нажатию ESC
    - Системным прерываниям
    """
    game = GameEngine(config=config)
    logger = StaticLoader.getLogger(__name__)

    # --- Основной цикл ---
    clock = pygame.time.Clock()
    mainLoop = True
    logger.warning("Main Loop is starting")

    while mainLoop:

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                mainLoop = False
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE: # Выход по нажатию Escape
                    mainLoop = False


            game.map_view.handle_event(event)

            for element in reversed(game.ui_elements):
                if element.handle_event(event):
                    break # Если элемент обработал событие, дальше не передаем
        
        # --- Отрисовка ---
        game.window.fill(WHITE)       # Заливаем основное окно фоном
        for element in game.ui_elements:
            element.draw(game.ui_surface)

        game.window.blit(game.ui_surface, (0, 0)) # Рисуем поверх все элементы UI


        game.map_view.draw(game.window)


        # --- Обновление экрана ---
        pygame.display.flip()
        clock.tick(game.fps)

    # --- Завершение работы ---
    pygame.quit()
    sys.exit() # Убедимся, что скрипт завершается


if __name__ == '__main__':
    main()