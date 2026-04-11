import threading
import queue
import time
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
import torch.nn.functional as F
from collections import deque, namedtuple
import random
import copy
import math

from dataclasses import dataclass, field
import logging
import os
from typing import Set, Tuple, Dict, Optional, List

# --- Configuration ---

LOGS_PATH = "temp/logs.log"
os.makedirs(os.path.dirname(LOGS_PATH), exist_ok=True)
LOGGING_LEVEL = logging.INFO

logging.basicConfig(
    filename=LOGS_PATH,
    level=LOGGING_LEVEL,
    filemode="a",
    format="%(asctime)s %(levelname)s %(name)s %(message)s",
    encoding="utf-8",
)

# --- Object Codes ---
# Важно: Эти коды используются как индексы для Embedding слоя!
# Должны быть непрерывными от 0 до NUM_OBJECT_CODES - 1.

SURFACE_CODE = 0
PEACEFUL_CODE = 1
PREDATOR_CODE = 2
FOOD_CODE = 3
WATER_CODE = 4
OBSTACLE_CODE = 5  # Этот же код используется для PAD_VALUE


@dataclass
class Config:
    """
    Класс конфигурации, содержащий все основные параметры симуляции и обучения.

    Использует `@dataclass` для автоматического создания `__init__`, `__repr__` и т.д.
    """
    MAP_WIDTH: int = 60
    MAP_HEIGHT: int = 50
    VIEW_SIZE: int = 7
    PAD_VALUE: int = OBSTACLE_CODE  # Используем код препятствия для паддинга
    NUM_OBJECT_CODES: int = 6  # Количество уникальных кодов объектов (0-5)
    INITIAL_PEACEFUL: int = 20
    INITIAL_PREDATORS: int = 5
    INITIAL_FOOD: int = 40
    FOOD_SPAWN_RATE: float = 0.05
    MAX_FOOD: int = 60
    MAX_AGENTS_PER_TYPE: int = 100

    # Agent Settings
    AGENT_STATE_DIM: int = 1
    NUM_ACTIONS: int = 5
    MAX_HUNGER: float = 100.0
    HUNGER_DECAY_STEP: float = 0.1
    STARVATION_LIMIT: float = 0.0
    REPRODUCTION_THRESHOLD: float = 95.0
    PEACEFUL_DEATH_IN_WATER_STEPS: int = 10
    PREDATOR_DEATH_IN_WATER_STEPS: int = 10

    # Rewards
    REWARD_STEP: float = -0.01
    REWARD_EAT_FOOD: float = 20.0
    REWARD_EAT_PREY: float = 50.0
    REWARD_REPRODUCE: float = 30.0
    REWARD_DEATH: float = -100.0
    REWARD_APPROACH_FOOD_FACTOR: float = (
        0.5  # reward = factor * (sqrt(dist_before) - sqrt(dist_after))
    )
    REWARD_APPROACH_PREY_FACTOR: float = (
        0.7  # reward = factor * (sqrt(dist_before) - sqrt(dist_after))
    )
    REWARD_FLEE_PREDATOR_FACTOR: float = (
        0.6  # reward = factor * (sqrt(dist_after) - sqrt(dist_before))
    )
    REWARD_MOVE_TO_EMPTY: float = 0.001

    # RL Training
    DEVICE: str = "cuda" if torch.cuda.is_available() else "cpu"
    LR_PEACEFUL: float = 1e-4
    LR_PREDATOR: float = 1e-4
    BUFFER_SIZE: int = 50000
    BATCH_SIZE: int = 128
    GAMMA: float = 0.99
    EPS_START: float = 0.95
    EPS_END: float = 0.05
    EPS_DECAY_STEPS: int = 100000
    TARGET_UPDATE_FREQ: int = 1000
    TRAIN_FREQ: int = 4  # Train every 4 steps
    MIN_BUFFER_SIZE_FOR_TRAIN: int = 1000
    EMBEDDING_DIM: int = 8  # Размерность эмбеддинга для кодов объектов

    # --- PER Settings ---
    PER_ALPHA: float = 0.6  # Priority exponent (0=uniform, 1=fully prioritized)
    PER_BETA_START: float = 0.4  # Initial IS weight exponent
    PER_BETA_FRAMES: int = 100000  # Steps to anneal beta to 1.0
    PER_EPSILON: float = 1e-5  # Small value added to priorities to ensure non-zero prob

    # Simulation Control
    EPOCH_LENGTH_STEPS: int = 1000

    # Save/Load Settings
    SAVE_MODEL_DIR: str = "./saved_models"
    PEACEFUL_MODEL_FILENAME: str = "peaceful_model_final.pth"
    PREDATOR_MODEL_FILENAME: str = "predator_model_final.pth"

    # Visualization
    VISUALIZATION_UPDATE_RATE_HZ: float = 10.0


# --- Prioritized Replay Buffer ---

Experience = namedtuple(
    "Experience", ("state", "action", "reward", "next_state", "done")
)


# --- SumTree for PER --- # (Minor correction in retrieve logic)
class SumTree:
    """
    Структура данных Суммирующее Дерево (SumTree) для Prioritized Experience Replay.

    Это двоичное дерево, где каждый лист хранит приоритет одного элемента опыта,
    а каждый внутренний узел хранит сумму приоритетов своих дочерних узлов.
    Корень дерева хранит общую сумму всех приоритетов.
    Используется для эффективного семплирования элементов с вероятностью,
    пропорциональной их приоритету.
    """
    write_ptr: int = 0

    def __init__(self, capacity: int):
        """
        Инициализирует SumTree.

        Args:
            capacity (int): Максимальная вместимость дерева (количество листьев).
        """
        self.capacity = capacity
        self.tree = np.zeros(2 * capacity - 1)
        self.data = np.zeros(capacity, dtype=object)
        self.data_size = 0

    def _propagate(self, idx: int, change: float):
        """
        Распространяет изменение приоритета вверх по дереву.

        Args:
            idx (int): Индекс узла в массиве `tree`, с которого началось изменение.
            change (float): Величина изменения приоритета.
        """
        parent = (idx - 1) // 2
        self.tree[parent] += change
        if parent != 0:
            self._propagate(parent, change)

    def _retrieve(self, idx: int, s: float) -> int:
        """
        Находит индекс листа, соответствующий заданному значению семплирования.

        Args:
            idx (int): Текущий индекс узла в дереве (начинается с 0 - корня).
            s (float): Значение семплирования (случайное число от 0 до total_priority).

        Returns:
            int: Индекс листа в массиве `tree`.
        """
        left = 2 * idx + 1
        right = left + 1

        if left >= len(self.tree):
            return idx

        if s <= self.tree[left] or self.tree[right] < 1e-9:
            return self._retrieve(left, s)
        else:
            return self._retrieve(right, s - self.tree[left])

    def total(self) -> float:
        """
        Возвращает общую сумму приоритетов (значение корневого узла).

        Returns:
            float: Сумма всех приоритетов в дереве.
        """
        return self.tree[0]

    def add(self, priority: float, data: object):
        """
        Добавляет новый элемент опыта с указанным приоритетом.

        Если дерево заполнено, старые данные перезаписываются.

        Args:
            priority (float): Приоритет добавляемого элемента.
            data (object): Данные элемента опыта (например, Experience).
        """
        tree_idx = self.write_ptr + self.capacity - 1

        self.data[self.write_ptr] = data
        self.update(tree_idx, priority)

        self.write_ptr += 1
        if self.write_ptr >= self.capacity:
            self.write_ptr = 0

        if self.data_size < self.capacity:
            self.data_size += 1

    def update(self, tree_idx: int, priority: float):
        """
        Обновляет приоритет существующего элемента в дереве.

        Args:
            tree_idx (int): Индекс листа в массиве `tree`, приоритет которого нужно обновить.
            priority (float): Новый приоритет элемента.
        """
        if tree_idx < self.capacity - 1 or tree_idx >= len(self.tree):
            return

        change = priority - self.tree[tree_idx]
        self.tree[tree_idx] = priority
        self._propagate(tree_idx, change)

    def get_leaf(self, s: float) -> Tuple[int, float, object]:
        """
        Получает индекс листа, приоритет и данные для заданного значения семплирования.

        Args:
            s (float): Значение семплирования (случайное число от 0 до total_priority).

        Returns:
            Tuple[int, float, object]: Кортеж (индекс_листа_в_tree, приоритет, данные).
                                        Возвращает (-1, 0.0, None) если не удалось найти валидный лист.
        """
        idx = self._retrieve(0, s)
        data_idx = idx - self.capacity + 1
        if data_idx < 0 or data_idx >= self.data_size:
            if self.total() > 0:
                s_retry = random.uniform(0, self.total())
                idx_retry = self._retrieve(0, s_retry)
                data_idx_retry = idx_retry - self.capacity + 1
                if 0 <= data_idx_retry < self.data_size:
                    return idx_retry, self.tree[idx_retry], self.data[data_idx_retry]
            return -1, 0.0, None

        return idx, self.tree[idx], self.data[data_idx]

    def __len__(self) -> int:
        """
        Возвращает текущее количество элементов в дереве.

        Returns:
            int: Количество хранимых элементов опыта.
        """
        return self.data_size


# --- PrioritizedReplayBuffer Class --- #
class PrioritizedReplayBuffer:
    """
    Приоритезированный буфер воспроизведения опыта (Prioritized Experience Replay - PER).

    Хранит переходы (опыт) и позволяет семплировать их с вероятностью,
    пропорциональной их TD-ошибке (приоритету). Это помогает модели
    быстрее учиться на более "важных" примерах.
    Также реализует Importance Sampling (IS) для коррекции смещения,
    вносимого приоритезированным семплированием.
    """
    def __init__(
        self,
        capacity: int,
        alpha: float,
        beta_start: float,
        beta_frames: int,
        epsilon: float,
    ):
        """
        Инициализирует PrioritizedReplayBuffer.

        Args:
            capacity (int): Максимальная вместимость буфера.
            alpha (float): Экспонента для расчета приоритета (0=равномерное, 1=полностью приоритезированное).
            beta_start (float): Начальное значение экспоненты для весов Importance Sampling (IS).
            beta_frames (int): Количество шагов, за которое beta линейно отжигается до 1.0.
            epsilon (float): Малое значение, добавляемое к ошибке TD для обеспечения ненулевого приоритета.
        """
        self.tree = SumTree(capacity)
        self.capacity = capacity
        self.alpha = alpha
        self.beta_start = beta_start
        self.beta_frames = beta_frames
        self.epsilon = epsilon
        self._max_priority = 1.0

    def _get_priority(self, error: float) -> float:
        """
        Рассчитывает приоритет на основе абсолютной TD-ошибки.

        Args:
            error (float): TD-ошибка.

        Returns:
            float: Рассчитанный приоритет.
        """
        return (abs(error) + self.epsilon) ** self.alpha

    def push(self, error: Optional[float], *args):
        """
        Сохраняет элемент опыта и его приоритет в буфере.

        Если `error` равен None (при первом добавлении, когда TD-ошибка еще не известна),
        используется максимальный известный приоритет.

        Args:
            error (Optional[float]): TD-ошибка, связанная с этим опытом. None, если добавляется впервые.
            *args: Аргументы для создания объекта `Experience` (state, action, reward, next_state, done).
        """
        priority = self._max_priority if error is None else self._get_priority(error)
        experience = Experience(*args)
        self.tree.add(priority, experience)

    def sample(
        self, batch_size: int, global_step: int
    ) -> Optional[Tuple[List[Experience], List[int], List[float]]]:
        """
        Семплирует `batch_size` элементов опыта на основе их приоритетов.

        Также рассчитывает веса Importance Sampling (IS).

        Args:
            batch_size (int): Размер батча для семплирования.
            global_step (int): Текущий глобальный шаг обучения (используется для отжига beta).

        Returns:
            Optional[Tuple[List[Experience], List[int], List[float]]]:
                Кортеж (список_опыта, список_индексов_в_дереве, список_весов_IS).
                Возвращает None, если в буфере недостаточно элементов для семплирования.
        """
        if len(self) < batch_size:
            return None

        batch = []
        indices = []
        segment = self.tree.total() / batch_size
        priorities = []

        beta = self.beta_start + (1.0 - self.beta_start) * min(
            1.0, global_step / self.beta_frames
        )

        attempts = 0
        max_attempts = batch_size * 2

        while len(batch) < batch_size and attempts < max_attempts:
            i = len(batch)
            a = segment * i
            b = segment * (i + 1)
            total_p = self.tree.total()
            if total_p <= 0:
                return None

            s = random.uniform(a, min(b, total_p))
            idx, p, data = self.tree.get_leaf(s)

            if idx != -1 and isinstance(data, Experience):
                if idx not in indices:
                    priorities.append(p)
                    batch.append(data)
                    indices.append(idx)
                # logging.debug(f"PER sample discarded invalid data (idx={idx}) from get_leaf.")

            attempts += 1

        if len(batch) < batch_size:
            # logging.warning(f"PER sampling failed to collect a full batch ({len(batch)}/{batch_size}) after {attempts} attempts.")
            return None

        sampling_probabilities = np.array(priorities) / self.tree.total()
        is_weights = np.power(
            self.tree.data_size * sampling_probabilities + 1e-8, -beta
        )
        is_weights /= is_weights.max() + 1e-8

        return batch, indices, is_weights

    def update_priorities(self, batch_indices: List[int], batch_errors: List[float]):
        """
        Обновляет приоритеты семплированных элементов опыта на основе их новых TD-ошибок.

        Args:
            batch_indices (List[int]): Список индексов элементов в SumTree, приоритеты которых нужно обновить.
            batch_errors (List[float]): Список соответствующих TD-ошибок.
        """
        if len(batch_indices) != len(batch_errors):
            logging.error(
                f"PER update_priorities size mismatch: {len(batch_indices)} indices vs {len(batch_errors)} errors."
            )
            return
        for idx, error in zip(batch_indices, batch_errors):
            if idx < 0:
                continue
            priority = self._get_priority(error)
            self.tree.update(idx, priority)
            self._max_priority = max(self._max_priority, priority)

    def clear(self):
        """
        Очищает буфер воспроизведения, удаляя все сохраненные элементы.
        """
        self.tree = SumTree(self.capacity)
        self._max_priority = 1.0
        logging.debug("Prioritized Replay buffer cleared.")

    def __len__(self):
        """
        Возвращает текущее количество элементов в буфере.

        Returns:
            int: Количество элементов.
        """
        return len(self.tree)


# --- Embedding + Dueling DQN Model (Corrected __init__ syntax) ---
class EmbeddingDuelingAgentNN(nn.Module):
    """
    Нейронная сеть для агента, использующая Embedding для локального вида,
    обработку дополнительного вектора состояния и архитектуру Dueling DQN.

    Локальный вид (матрица кодов объектов) подается в Embedding слой, затем в CNN.
    Дополнительный вектор состояния (например, голод) обрабатывается отдельной MLP.
    Выходы CNN и MLP объединяются и подаются в общую часть сети,
    а затем в раздельные головы для оценки функции ценности состояния (Value)
    и функции преимущества (Advantage) для каждого действия.
    """
    def __init__(
        self, view_size, state_dim, num_actions, num_object_codes, embedding_dim
    ):
        """
        Инициализирует модель EmbeddingDuelingAgentNN.

        Args:
            view_size (int): Размер квадратного локального вида агента (например, 7 для 7x7).
            state_dim (int): Размерность дополнительного вектора состояния агента (например, 1 для голода).
            num_actions (int): Количество возможных действий агента.
            num_object_codes (int): Количество уникальных кодов объектов на карте (для Embedding слоя).
            embedding_dim (int): Размерность вектора эмбеддинга для каждого кода объекта.
        """
        super(EmbeddingDuelingAgentNN, self).__init__()
        self.view_size = view_size
        self.num_actions = num_actions
        self.embedding_dim = embedding_dim

        self.embedding = nn.Embedding(num_object_codes, embedding_dim)

        # CNN for embedded local view
        self.conv1 = nn.Conv2d(embedding_dim, 16, kernel_size=3, stride=1, padding=1)
        self.conv2 = nn.Conv2d(16, 32, kernel_size=3, stride=1, padding=1)

        # Calculate flattened size after CNN
        with torch.no_grad():
            dummy_input_indices = torch.zeros(1, view_size, view_size, dtype=torch.long)
            embedded_dummy = self.embedding(dummy_input_indices)
            embedded_dummy = embedded_dummy.permute(0, 3, 1, 2)
            conv_out = self.conv2(F.relu(self.conv1(embedded_dummy)))
            self.flattened_size = int(np.prod(conv_out.shape[1:]))

        self.fc1_features = nn.Linear(self.flattened_size, 64)
        self.fc1_state = nn.Linear(state_dim, 16)

        self.fc_combined_features = nn.Linear(64 + 16, 128)

        # --- Dueling Heads ---
        self.fc_value = nn.Linear(128, 1)
        self.fc_advantage = nn.Linear(128, num_actions)

    def forward(self, local_view_indices, agent_state):
        """
        Прямой проход данных через сеть для получения Q-значений.

        Args:
            local_view_indices (torch.Tensor): Тензор с индексами кодов объектов в локальном виде.
                                               Ожидаемая форма: (batch_size, view_size, view_size), dtype=torch.long.
            agent_state (torch.Tensor): Тензор с дополнительным вектором состояния агента.
                                        Ожидаемая форма: (batch_size, state_dim), dtype=torch.float.

        Returns:
            torch.Tensor: Тензор Q-значений для каждого действия. Форма: (batch_size, num_actions).
        """
        if local_view_indices.dtype != torch.long:
            local_view_indices = local_view_indices.long()
        if local_view_indices.dim() == 2:
            local_view_indices = local_view_indices.unsqueeze(0)
        if agent_state.dim() == 1:
            agent_state = agent_state.unsqueeze(0)

        if agent_state.dtype != torch.float32:
            agent_state = agent_state.float()

        x_view_embedded = self.embedding(local_view_indices)
        x_view_embedded = x_view_embedded.permute(0, 3, 1, 2)

        x_view = F.relu(self.conv1(x_view_embedded))
        x_view = F.relu(self.conv2(x_view))
        x_view = x_view.reshape(x_view.size(0), -1)
        if x_view.shape[1] != self.flattened_size:
            logging.error(
                f"Flattened size mismatch! Expected {self.flattened_size}, got {x_view.shape[1]}"
            )
            self.fc1_features = nn.Linear(x_view.shape[1], 64).to(
                local_view_indices.device
            )

        x_view = F.relu(self.fc1_features(x_view))

        x_state = F.relu(self.fc1_state(agent_state))

        x_combined = torch.cat((x_view, x_state), dim=1)
        x_features = F.relu(self.fc_combined_features(x_combined))

        value = self.fc_value(x_features)
        advantage = self.fc_advantage(x_features)
        mean_advantage = advantage.mean(dim=1, keepdim=True)
        q_values = value + (advantage - mean_advantage)

        return q_values


# --- Agent Classes (Corrected __init__ syntax) ---
@dataclass
class Agent:
    """
    Базовый класс для всех агентов в симуляции.

    Содержит общие атрибуты и методы для агентов, такие как ID, позиция,
    уровень голода, тип, конфигурация и статус (жив/мертв).
    """
    id: int
    pos: tuple[int, int]
    hunger: float
    type_code: int
    config: Config
    alive: bool = True
    steps_in_water: int = 0

    def normalize_hunger(self) -> float:
        """
        Нормализует текущий уровень голода к диапазону [0.0, 1.0].

        Returns:
            float: Нормализованный уровень голода.
        """
        return np.clip(self.hunger / self.config.MAX_HUNGER, 0.0, 1.0)

    def get_state_vector(self) -> np.ndarray:
        """
        Возвращает вектор состояния агента (например, нормализованный голод).

        Этот вектор используется как дополнительный вход для нейронной сети
        вместе с локальным видом.

        Returns:
            np.ndarray: Вектор состояния агента (float32).
        """
        return np.array([self.normalize_hunger()], dtype=np.float32)


class PeacefulAgent(Agent):
    """
    Класс, представляющий мирного агента (травоядного).

    Наследуется от базового класса `Agent`.
    """
    def __init__(self, id, pos, hunger, config):
        """
        Инициализирует мирного агента.

        Args:
            id (int): Уникальный идентификатор агента.
            pos (tuple[int, int]): Начальная позиция (y, x) агента на карте.
            hunger (float): Начальный уровень голода агента.
            config (Config): Объект конфигурации симуляции.
        """
        super().__init__(
            id=id, pos=pos, hunger=hunger, type_code=PEACEFUL_CODE, config=config
        )


class PredatorAgent(Agent):
    """
    Класс, представляющий хищного агента.

    Наследуется от базового класса `Agent`.
    """
    def __init__(self, id, pos, hunger, config):
        """
        Инициализирует хищного агента.

        Args:
            id (int): Уникальный идентификатор агента.
            pos (tuple[int, int]): Начальная позиция (y, x) агента на карте.
            hunger (float): Начальный уровень голода агента.
            config (Config): Объект конфигурации симуляции.
        """
        super().__init__(
            id=id, pos=pos, hunger=hunger, type_code=PREDATOR_CODE, config=config
        )


# --- Environment (Corrected __init__, approach reward fix) ---
class Environment:
    """
    Класс среды симуляции.

    Управляет состоянием мира, включая карту, агентов, еду.
    Обрабатывает действия агентов, обновляет их состояния, рассчитывает вознаграждения
    и предоставляет информацию для обучения (состояния, вознаграждения, флаги завершения).
    """
    def __init__(self, initial_map_with_entities: np.ndarray, config: Config):
        """
        Инициализирует среду симуляции.

        Args:
            initial_map_with_entities (np.ndarray): Начальная карта мира с размещенными сущностями
                                                    (агенты, еда, препятствия, вода).
                                                    Коды объектов должны соответствовать определенным константам.
            config (Config): Объект конфигурации симуляции.
        """
        self.config = config
        self.initial_map_ref = initial_map_with_entities.copy()
        self.height, self.width = self.initial_map_ref.shape
        self.agents: Dict[int, Agent] = {}
        self.peaceful_agents: Dict[int, Agent] = {}
        self.predator_agents: Dict[int, Agent] = {}
        self.food_locations: Set[Tuple[int, int]] = set()
        self._next_agent_id = 0
        self.base_map: np.ndarray = np.zeros_like(self.initial_map_ref, dtype=np.int8)
        self.shared_map: np.ndarray = np.zeros_like(self.initial_map_ref, dtype=np.int8)

        self._initialize_environment_state(self.initial_map_ref)

    def _get_next_id(self):
        """
        Генерирует и возвращает следующий уникальный ID для агента.

        Returns:
            int: Уникальный ID.
        """
        self._next_agent_id += 1
        return self._next_agent_id

    def _is_valid(self, y, x):
        """
        Проверяет, находятся ли координаты (y, x) в пределах карты.

        Args:
            y (int): Координата Y.
            x (int): Координата X.

        Returns:
            bool: True, если координаты валидны, иначе False.
        """
        return 0 <= y < self.height and 0 <= x < self.width

    def _get_terrain_code_from_base(self, y, x):
        """
        Возвращает код типа местности из базовой карты (base_map) для указанных координат.

        Если координаты невалидны, возвращает OBSTACLE_CODE.

        Args:
            y (int): Координата Y.
            x (int): Координата X.

        Returns:
            int: Код типа местности (SURFACE_CODE, WATER_CODE, OBSTACLE_CODE).
        """
        if not self._is_valid(y, x):
            return OBSTACLE_CODE
        return self.base_map[y, x]

    def _is_obstacle_on_map(self, y, x):
        """
        Проверяет, является ли ячейка (y, x) на текущей карте (shared_map)
        непроходимым препятствием (статическое препятствие или другой агент).

        Args:
            y (int): Координата Y.
            x (int): Координата X.

        Returns:
            bool: True, если ячейка непроходима, иначе False.
        """
        return not self._is_valid(y, x) or self.shared_map[y, x] in [
            OBSTACLE_CODE,
            PEACEFUL_CODE,
            PREDATOR_CODE,
        ]

    def _is_water_from_base(self, y, x):
        """
        Проверяет, является ли ячейка (y, x) водой на базовой карте.

        Args:
            y (int): Координата Y.
            x (int): Координата X.

        Returns:
            bool: True, если ячейка - вода, иначе False.
        """
        return self._is_valid(y, x) and self.base_map[y, x] == WATER_CODE

    def _add_agent_to_dicts(self, agent: Agent):
        """
        Добавляет агента в соответствующие словари (`agents`, `peaceful_agents` или `predator_agents`).

        Args:
            agent (Agent): Объект агента для добавления.
        """
        self.agents[agent.id] = agent
        if agent.type_code == PEACEFUL_CODE:
            self.peaceful_agents[agent.id] = agent
        elif agent.type_code == PREDATOR_CODE:
            self.predator_agents[agent.id] = agent

    def _remove_agent(self, agent_id: int):
        """
        Удаляет агента из всех управляющих словарей и помечает его как неживого.

        Args:
            agent_id (int): ID агента для удаления.

        Returns:
            Optional[Agent]: Удаленный объект агента, если он был найден, иначе None.
        """
        agent = self.agents.pop(agent_id, None)
        if agent:
            agent.alive = False
            self.peaceful_agents.pop(agent_id, None)
            self.predator_agents.pop(agent_id, None)
        return agent

    def _initialize_environment_state(self, map_to_process: np.ndarray):
        """
        Полностью инициализирует или переинициализирует состояние среды на основе предоставленной карты.

        Очищает текущих агентов, еду. Создает `base_map` (только рельеф)
        и `shared_map` (рельеф + сущности). Размещает агентов и еду
        в соответствии с кодами на `map_to_process`.

        Args:
            map_to_process (np.ndarray): Карта, используемая для инициализации.
        """
        logging.debug("Initializing environment state from provided map...")
        self.agents.clear()
        self.peaceful_agents.clear()
        self.predator_agents.clear()
        self.food_locations.clear()
        self._next_agent_id = 0

        self.shared_map = map_to_process.copy().astype(np.int8)
        self.base_map = np.full_like(map_to_process, SURFACE_CODE, dtype=np.int8)

        for r in range(self.height):
            for c in range(self.width):
                code = map_to_process[r, c]
                pos = (r, c)

                if code == WATER_CODE:
                    self.base_map[pos] = WATER_CODE
                elif code == OBSTACLE_CODE:
                    self.base_map[pos] = OBSTACLE_CODE

                if code == PEACEFUL_CODE:
                    agent = PeacefulAgent(
                        self._get_next_id(),
                        pos,
                        self.config.MAX_HUNGER * 0.8,
                        self.config,
                    )
                    self._add_agent_to_dicts(agent)
                elif code == PREDATOR_CODE:
                    agent = PredatorAgent(
                        self._get_next_id(),
                        pos,
                        self.config.MAX_HUNGER * 0.8,
                        self.config,
                    )
                    self._add_agent_to_dicts(agent)
                elif code == FOOD_CODE:
                    self.food_locations.add(pos)

        num_p = len(self.peaceful_agents)
        num_r = len(self.predator_agents)
        num_f = len(self.food_locations)
        logging.info(
            f"Environment state initialized. Peaceful: {num_p}, Predators: {num_r}, Food: {num_f}"
        )

    def reset(self):
        """
        Сбрасывает состояние симуляции к начальному, используя исходную карту,
        предоставленную при инициализации `Environment`.

        Все агенты, еда и их состояния будут восстановлены к первоначальным.
        """
        logging.warning("!!! Resetting simulation environment using initial map !!!")
        self._initialize_environment_state(self.initial_map_ref)
        logging.warning("!!! Simulation environment reset complete !!!")

    def _spawn_food(self):
        """
        Пытается разместить новую еду на карте с вероятностью `FOOD_SPAWN_RATE`.

        Еда появляется на случайной свободной ячейке типа SURFACE_CODE,
        если текущее количество еды меньше `MAX_FOOD`.
        """
        if len(self.food_locations) < self.config.MAX_FOOD:
            if random.random() < self.config.FOOD_SPAWN_RATE:
                attempts = 0
                while attempts < 10:
                    y, x = random.randint(0, self.height - 1), random.randint(
                        0, self.width - 1
                    )
                    if (
                        self._is_valid(y, x)
                        and self.base_map[y, x] == SURFACE_CODE
                        and self.shared_map[y, x] == SURFACE_CODE
                    ):
                        self.shared_map[y, x] = FOOD_CODE
                        self.food_locations.add((y, x))
                        break
                    attempts += 1

    def _dist_sq(self, pos1, pos2):
        """
        Рассчитывает квадрат евклидова расстояния между двумя точками.

        Args:
            pos1 (tuple[int, int]): Координаты первой точки (y1, x1).
            pos2 (tuple[int, int]): Координаты второй точки (y2, x2).

        Returns:
            float: Квадрат расстояния.
        """
        return (pos1[0] - pos2[0]) ** 2 + (pos1[1] - pos2[1]) ** 2

    def _find_closest(self, agent_pos, target_set):
        """
        Находит ближайшую цель из заданного множества целей к позиции агента.

        Args:
            agent_pos (tuple[int, int]): Позиция агента (y, x).
            target_set (Set[Tuple[int, int]]): Множество позиций целей (y, x).

        Returns:
            Tuple[Optional[Tuple[int, int]], float]:
                Кортеж (позиция_ближайшей_цели, квадрат_расстояния_до_нее).
                Если `target_set` пусто, возвращает (None, float('inf')).
        """
        min_dist_sq = float("inf")
        closest_target = None
        if not target_set:
            return None, min_dist_sq
        for target_pos in target_set:
            dist_sq = self._dist_sq(agent_pos, target_pos)
            if dist_sq < min_dist_sq:
                min_dist_sq = dist_sq
                closest_target = target_pos
        return closest_target, min_dist_sq

    def get_padded_map(self) -> np.ndarray:
        """
        Возвращает копию `shared_map` с паддингом по краям.

        Размер паддинга определяется `VIEW_SIZE // 2`.
        Значение для паддинга берется из `config.PAD_VALUE`.

        Returns:
            np.ndarray: Карта с паддингом.
        """
        pad_width = self.config.VIEW_SIZE // 2
        return np.pad(
            self.shared_map,
            pad_width=pad_width,
            mode="constant",
            constant_values=self.config.PAD_VALUE,
        )

    def get_local_view(
        self, agent_pos: tuple[int, int], padded_map: np.ndarray
    ) -> np.ndarray:
        """
        Извлекает локальный вид агента из карты с паддингом.

        Размер вида определяется `config.VIEW_SIZE`.
        Центр вида соответствует позиции агента.

        Args:
            agent_pos (tuple[int, int]): Позиция агента (y, x) на оригинальной (не паддированной) карте.
            padded_map (np.ndarray): Карта с паддингом, из которой извлекается вид.

        Returns:
            np.ndarray: Локальный вид агента (матрица кодов объектов) типа int64.
        """
        y, x = agent_pos
        pad_width = self.config.VIEW_SIZE // 2
        center_y, center_x = y + pad_width, x + pad_width
        half_size = self.config.VIEW_SIZE // 2
        view = padded_map[
            center_y - half_size : center_y + half_size + 1,
            center_x - half_size : center_x + half_size + 1,
        ]
        return view.astype(np.int64)

    def get_agent_state(self, agent_id: int) -> Optional[Tuple[np.ndarray, np.ndarray]]:
        """
        Возвращает полное состояние для указанного агента.

        Состояние включает локальный вид и дополнительный вектор состояния (например, голод).

        Args:
            agent_id (int): ID агента.

        Returns:
            Optional[Tuple[np.ndarray, np.ndarray]]:
                Кортеж (локальный_вид, вектор_состояния).
                Локальный_вид: np.ndarray формы (VIEW_SIZE, VIEW_SIZE), dtype=int64.
                Вектор_состояния: np.ndarray формы (AGENT_STATE_DIM,), dtype=float32.
                Возвращает None, если агент не найден или не жив.
        """
        agent = self.agents.get(agent_id)
        if not agent or not agent.alive:
            return None
        padded_map = self.get_padded_map()
        local_view_indices = self.get_local_view(agent.pos, padded_map)
        state_vector = agent.get_state_vector()
        return local_view_indices, state_vector

    def step(
        self, actions: Dict[int, int]
    ) -> Tuple[
        Dict[int, Optional[Tuple[np.ndarray, np.ndarray]]],
        Dict[int, float],
        Dict[int, bool],
    ]:
        """
        Выполняет один шаг симуляции на основе действий, предпринятых агентами.

        Процесс шага:
        1. Обновление статуса агентов (голод, утопление, смерть от голода).
        2. Расчет предполагаемых ходов агентов на основе их действий.
        3. Разрешение конфликтов за пустые клетки (если два агента хотят на одну и ту же).
        4. Обработка взаимодействий на финальных позициях (поедание еды, атака других агентов).
        5. Обновление карты (удаление съеденной еды, размещение агентов на новых позициях).
        6. Расчет вознаграждений за приближение/удаление от целей (еда, хищники, жертвы).
        7. Обработка размножения агентов.
        8. Удаление мертвых агентов с карты.
        9. Спавн новой еды.
        10. Формирование следующих состояний для агентов, которые действовали.

        Args:
            actions (Dict[int, int]): Словарь {agent_id: action_code}, содержащий действия
                                      для каждого активного агента.

        Returns:
            Tuple[
                Dict[int, Optional[Tuple[np.ndarray, np.ndarray]]], # next_states
                Dict[int, float],                                   # rewards
                Dict[int, bool]                                     # dones
            ]:
                - `next_states`: Словарь {agent_id: (local_view, state_vector)} для следующего состояния
                                 каждого агента, совершившего действие. None, если агент умер.
                - `rewards`: Словарь {agent_id: reward_value} для каждого агента, совершившего действие.
                - `dones`: Словарь {agent_id: is_done} (True, если агент умер или эпизод для него завершен).
        """
        rewards = {}
        dones = {}
        next_states = {}
        agent_intended_positions = {} # Renamed for clarity
        agents_to_remove = set()
        agents_to_add = []
        events = {id: [] for id in self.agents if self.agents[id].alive} # Include only alive agents initially

        # --- State Before Movement ---
        current_agent_positions = {
            agent.pos: agent.id
            for agent_id, agent in self.agents.items()
            if agent.alive
        }
        agent_ids_acted = list(actions.keys())
        current_food_locations = self.food_locations.copy()
        # Get positions of agents ALIVE at the start of the step
        current_peaceful_pos = { a.pos for id, a in self.peaceful_agents.items() if a.alive }
        current_predator_pos = { a.pos for id, a in self.predator_agents.items() if a.alive }


        # Calculate initial closest targets for approach/flee rewards later
        closest_targets_before = {}
        for agent_id in agent_ids_acted:
            agent = self.agents.get(agent_id)
            if not agent or not agent.alive:
                continue
            # Ensure agent_id is actually in actions dict if agent exists
            if agent_id not in actions:
                logging.warning(f"Agent {agent_id} exists but has no action in step dictionary. Skipping.")
                continue

            if agent.type_code == PEACEFUL_CODE:
                _, dist_sq_food = self._find_closest(agent.pos, current_food_locations)
                _, dist_sq_pred = self._find_closest(agent.pos, current_predator_pos)
                closest_targets_before[agent_id] = {
                    "food_dist_sq": dist_sq_food,
                    "pred_dist_sq": dist_sq_pred,
                }
            elif agent.type_code == PREDATOR_CODE:
                _, dist_sq_prey = self._find_closest(agent.pos, current_peaceful_pos)
                closest_targets_before[agent_id] = {"prey_dist_sq": dist_sq_prey}

        # --- 1. Agent Status Update & Calculate Intended Moves ---
        for agent_id in agent_ids_acted:
            agent = self.agents.get(agent_id)
            # Double check agent exists and is alive before processing
            if not agent or not agent.alive:
                # Remove agent_id if it was somehow included but agent died before actions applied
                if agent_id in closest_targets_before: del closest_targets_before[agent_id]
                continue

            # Initial reward and hunger decay applied only if agent is alive at start
            rewards[agent_id] = self.config.REWARD_STEP
            agent.hunger -= self.config.HUNGER_DECAY_STEP

            if agent.hunger <= self.config.STARVATION_LIMIT:
                events.setdefault(agent_id, []).append("starved") # Use setdefault
                agents_to_remove.add(agent_id)
                # Don't calculate intended position if starving
                continue # Skip to next agent

            # Calculate intended position based on action
            action = actions.get(agent_id, 0) # Default to stay if action missing (shouldn't happen with check above)
            y, x = agent.pos
            ny, nx = y, x
            if action == 1: ny -= 1
            elif action == 2: ny += 1
            elif action == 3: nx -= 1
            elif action == 4: nx += 1

            # Store the raw intended position (ignoring conflicts for now)
            agent_intended_positions[agent_id] = (ny, nx)


        # --- 2. Resolve Conflicts for EMPTY squares & Determine Actual Moves ---
        # This stage decides where each agent *actually* ends up before interactions are calculated.
        # It handles: moving off map, moving into OBSTACLE, two agents wanting the same EMPTY square.
        # It DOES NOT prevent moving onto a square currently occupied by another agent yet.
        final_positions = {}
        occupied_next_step = {} # Tracks which EMPTY squares are claimed

        processed_agent_ids = list(agent_intended_positions.keys()) # Only process agents who calculated an intended move
        random.shuffle(processed_agent_ids)

        for agent_id in processed_agent_ids:
            agent = self.agents.get(agent_id) # Agent is guaranteed to exist and be non-starving here

            current_pos = agent.pos
            intended_ny, intended_nx = agent_intended_positions[agent_id]
            final_ny, final_nx = intended_ny, intended_nx # Start assuming intended move is possible

            # A. Check for moving off map or into static obstacle on BASE map
            if not self._is_valid(intended_ny, intended_nx) or \
               self.base_map[intended_ny, intended_nx] == OBSTACLE_CODE:
                final_ny, final_nx = current_pos # Cannot move, stay put

            # B. Check for conflict over the *same target square* with another agent THIS step
            # This is primarily for preventing two agents moving to the SAME *empty* square.
            # It does NOT yet prevent moving onto an occupied square.
            elif (final_ny, final_nx) != current_pos: # Only check conflicts if actually trying to move
                target_pos = (final_ny, final_nx)
                conflicting_agent_id = occupied_next_step.get(target_pos)

                if conflicting_agent_id is not None:
                    # Conflict! Another agent already claimed this target square. Revert to current position.
                    final_ny, final_nx = current_pos
                else:
                    # No conflict *yet*. Temporarily claim this square.
                    # We claim it regardless of whether it's currently occupied by another agent.
                    # That occupied case is handled by interactions later.
                     occupied_next_step[target_pos] = agent_id

            # Store the resolved final position for this agent
            final_positions[agent_id] = (final_ny, final_nx)


        # --- 3. Process Interactions at Final Positions & Update Status ---
        # Now we know where each agent *will* be. Check interactions based on these final positions.
        map_updates = {} # Store changes needed for the shared_map
        food_eaten_coords = set()
        prey_eaten_ids = set()

        # We iterate again, potentially can optimize later, but clarity first.
        # Iterate using original agent_ids_acted to include those who might have starved but need rewards/dones set.
        processed_agent_ids_for_interaction = list(agent_ids_acted)
        # No shuffle needed here, outcome should be deterministic based on final_positions

        for agent_id in processed_agent_ids_for_interaction:
            agent = self.agents.get(agent_id)
            if not agent: continue # Agent might have been removed if not in initial dict

            # Handle agents marked for removal earlier (starvation)
            if agent_id in agents_to_remove:
                event_list = events.get(agent_id, [])
                death_reward_penalty = self.config.REWARD_DEATH
                # Add starvation penalty specific logic if needed, otherwise general death penalty
                rewards[agent_id] = rewards.get(agent_id, 0) + death_reward_penalty # Add to initial step reward
                dones[agent_id] = True
                continue # Skip interaction checks for starving agents

            # Agent is alive, get its final position
            final_pos = final_positions.get(agent_id)
            if final_pos is None:
                 # Should not happen if agent wasn't starving, but handle defensively
                 logging.error(f"Agent {agent_id} is alive but has no final_pos. Setting done=True.")
                 dones[agent_id] = True
                 agents_to_remove.add(agent_id) # Mark for removal
                 continue

            agent.pos = final_pos # Tentatively update position for interaction checks
            current_pos = agent.pos # Use the new final position
            moved = final_pos != agent_intended_positions.get(agent_id, final_pos) # Did the agent move from its original spot?


            # --- Check Drowning ---
            target_is_water = self._is_water_from_base(current_pos[0], current_pos[1])
            if target_is_water:
                agent.steps_in_water += 1
                drown_limit = (
                    self.config.PREDATOR_DEATH_IN_WATER_STEPS
                    if agent.type_code == PREDATOR_CODE
                    else self.config.PEACEFUL_DEATH_IN_WATER_STEPS
                )
                if agent.steps_in_water >= drown_limit:
                    events.setdefault(agent_id, []).append("drowned_water")
                    agents_to_remove.add(agent_id)
                    # Apply death penalty now, will be handled fully later
                    rewards[agent_id] = rewards.get(agent_id, 0) + self.config.REWARD_DEATH
                    dones[agent_id] = True
                    continue # Skip other interactions if drowned
            elif moved: # Reset counter if moved to non-water
                agent.steps_in_water = 0

            # --- Check Interactions (Eat Food / Eat Prey) ---
            # Interactions happen based on the agent's FINAL position for this step.
            interaction_target_code = self.shared_map[current_pos[0], current_pos[1]] # What was on the map BEFORE this step's updates?

            # --- Peaceful Eats Food ---
            if (agent.type_code == PEACEFUL_CODE and
                interaction_target_code == FOOD_CODE and
                current_pos not in food_eaten_coords): # Check if food is still available this step

                agent.hunger = min(
                    agent.hunger + self.config.REWARD_EAT_FOOD * 0.8,
                    self.config.MAX_HUNGER,
                )
                events.setdefault(agent_id, []).append("ate_food")
                rewards[agent_id] = rewards.get(agent_id, 0) + self.config.REWARD_EAT_FOOD
                food_eaten_coords.add(current_pos)
                # Map update: food removed, replaced by base terrain (or agent later)
                map_updates[current_pos] = self._get_terrain_code_from_base(*current_pos)

            # --- Predator Eats Peaceful ---
            elif (agent.type_code == PREDATOR_CODE and
                  interaction_target_code == PEACEFUL_CODE):

                # Find the ID of the peaceful agent that WAS at this position at the start of the step
                prey_agent_id = current_agent_positions.get(current_pos)

                # Check if this prey is valid and wasn't already eaten/removed
                if (prey_agent_id is not None and
                    prey_agent_id != agent_id and # Cannot eat self
                    prey_agent_id not in agents_to_remove and
                    prey_agent_id not in prey_eaten_ids):

                    target_agent = self.agents.get(prey_agent_id)
                    # Double check it's the correct type and actually alive before this interaction
                    if target_agent and target_agent.type_code == PEACEFUL_CODE and target_agent.alive:
                        # Successful Eat!
                        agent.hunger = min(
                            agent.hunger + self.config.REWARD_EAT_PREY * 0.8,
                            self.config.MAX_HUNGER,
                        )
                        events.setdefault(agent_id, []).append("ate_prey")
                        rewards[agent_id] = rewards.get(agent_id, 0) + self.config.REWARD_EAT_PREY

                        # Handle the prey
                        events.setdefault(prey_agent_id, []).append("eaten")
                        agents_to_remove.add(prey_agent_id)
                        prey_eaten_ids.add(prey_agent_id)
                        # Apply death penalty to prey now
                        rewards[prey_agent_id] = rewards.get(prey_agent_id, 0) + self.config.REWARD_DEATH * 1.1 # Eaten penalty
                        dones[prey_agent_id] = True

                        # Map update: prey removed, replaced by base terrain (predator occupies later)
                        map_updates[current_pos] = self._get_terrain_code_from_base(*current_pos)


        # --- 4. Apply Map Updates & Finalize Agent Positions ---
        # Update map based on interactions (eating removing things)
        self.food_locations.difference_update(food_eaten_coords)
        # Clear eaten prey from their original squares if not already updated
        processed_agents_for_placement = list(self.agents.keys())
        for agent_id in processed_agents_for_placement:
            agent = self.agents.get(agent_id)
            # Skip if agent doesn't exist (e.g., removed mid-step somehow before this phase)
            # Or if agent is already marked for removal (e.g. starved, drowned, eaten)
            if not agent or agent_id in agents_to_remove:
                continue

            # Agent is alive and not marked for immediate removal
            final_pos = final_positions.get(agent_id)
            if final_pos:
                # --- Correctly find the starting position ---
                start_pos = None
                for pos_key, id_val in current_agent_positions.items():
                     if id_val == agent_id:
                         start_pos = pos_key
                         break
                # --- End correct find ---

                moved = start_pos and final_pos != start_pos

                # Clear the agent's previous position IF it moved
                if moved:
                     moved_from = start_pos # The square the agent started on

                     # Check if another *live* agent is landing on this 'moved_from' spot
                     is_original_spot_now_occupied_by_another = False
                     for other_id, other_final_pos in final_positions.items():
                         # Check if another agent (not self, not being removed) targets the spot agent moved FROM
                         if other_id != agent_id and other_id not in agents_to_remove and other_final_pos == moved_from:
                             is_original_spot_now_occupied_by_another = True
                             break

                     # If the agent moved AND its starting spot is NOT the target of another live agent...
                     if not is_original_spot_now_occupied_by_another:
                         # ...clear the spot UNLESS it was already updated by an interaction
                         # (e.g., food eaten there by the agent before it moved - unlikely but possible).
                         # The most common case is clearing to base terrain.
                         if moved_from not in map_updates: # Avoid overwriting interaction updates like food eaten
                             map_updates[moved_from] = self._get_terrain_code_from_base(*moved_from)


                # Place the agent at its final position (overwrites terrain/previous content)
                map_updates[final_pos] = agent.type_code
                agent.pos = final_pos # Officially update agent's internal position attribute AFTER all checks
            else:
                 # This case should ideally not happen for alive agents not in agents_to_remove
                 logging.error(f"Agent {agent_id} alive/not removed, but missing final_pos in map update phase.")
                 agents_to_remove.add(agent_id) # Mark for removal if state is inconsistent
                 dones[agent_id] = True


        # Apply all collected map updates to the actual shared map
        for (y, x), code in map_updates.items():
            if self._is_valid(y, x):
                self.shared_map[y, x] = code

        # --- 5. Calculate Approach/Flee Rewards & Reproduction ---
        # This needs the *final* state after all movements and interactions.
        final_peaceful_pos = { a.pos for id, a in self.peaceful_agents.items() if a.alive and id not in agents_to_remove }
        final_predator_pos = { a.pos for id, a in self.predator_agents.items() if a.alive and id not in agents_to_remove }
        final_food_locations = self.food_locations.copy() # Food state after eating this step

        for agent_id in agent_ids_acted:
            agent = self.agents.get(agent_id)
            if not agent or agent_id in agents_to_remove or not agent.alive:
                # Ensure done is set if agent died during interaction phase but wasn't caught earlier
                if agent_id not in dones: dones[agent_id] = True
                continue # Skip reward calcs and reproduction for dead agents

            # Agent is alive, calculate rewards based on state change
            dones[agent_id] = False # Mark as not done if alive
            pos_after = agent.pos
            targets_before = closest_targets_before.get(agent_id, {})

            # --- Approach / Flee Rewards ---
            if agent.type_code == PEACEFUL_CODE:
                # Food Approach
                dist_sq_before_food = targets_before.get("food_dist_sq", float("inf"))
                _, dist_sq_after_food = self._find_closest(pos_after, final_food_locations)
                if dist_sq_after_food < dist_sq_before_food and dist_sq_before_food != float("inf"):
                    delta_dist = math.sqrt(dist_sq_before_food) - math.sqrt(dist_sq_after_food)
                    if delta_dist > 0:
                        rewards[agent_id] = rewards.get(agent_id, 0) + self.config.REWARD_APPROACH_FOOD_FACTOR * delta_dist

                # Predator Flee
                dist_sq_before_pred = targets_before.get("pred_dist_sq", float("inf"))
                _, dist_sq_after_pred = self._find_closest(pos_after, final_predator_pos)
                if dist_sq_after_pred > dist_sq_before_pred and dist_sq_before_pred != float("inf"):
                    delta_dist = math.sqrt(dist_sq_after_pred) - math.sqrt(dist_sq_before_pred)
                    if delta_dist > 0:
                        rewards[agent_id] = rewards.get(agent_id, 0) + self.config.REWARD_FLEE_PREDATOR_FACTOR * delta_dist

            elif agent.type_code == PREDATOR_CODE:
                # Prey Approach
                dist_sq_before_prey = targets_before.get("prey_dist_sq", float("inf"))
                _, dist_sq_after_prey = self._find_closest(pos_after, final_peaceful_pos) # Use final peaceful positions
                if dist_sq_after_prey < dist_sq_before_prey and dist_sq_before_prey != float("inf"):
                    delta_dist = math.sqrt(dist_sq_before_prey) - math.sqrt(dist_sq_after_prey)
                    if delta_dist > 0:
                        rewards[agent_id] = rewards.get(agent_id, 0) + self.config.REWARD_APPROACH_PREY_FACTOR * delta_dist

            # --- Reproduction ---
            if (agent.type_code == PEACEFUL_CODE and
                agent.hunger >= self.config.REPRODUCTION_THRESHOLD and
                len(self.peaceful_agents) < self.config.MAX_AGENTS_PER_TYPE):

                birth_pos = None
                # Check adjacent squares relative to the agent's *final* position
                potential_birth_spots = []
                for dy, dx in [(0, 1), (0, -1), (1, 0), (-1, 0), (0,0)]: # Check current square too? Maybe not. Let's keep original neighbours.
                     check_y, check_x = pos_after[0] + dy, pos_after[1] + dx
                     potential_pos = (check_y, check_x)
                     potential_birth_spots.append(potential_pos)
                random.shuffle(potential_birth_spots) # Randomize birth spot choice

                for potential_pos in potential_birth_spots:
                     check_y, check_x = potential_pos
                     # Check if valid, is surface, and is currently empty on the *final* map state
                     if (self._is_valid(check_y, check_x) and
                         self.base_map[potential_pos] == SURFACE_CODE and
                         self.shared_map[potential_pos] == SURFACE_CODE): # Check final map state
                           birth_pos = potential_pos
                           break # Found a spot

                if birth_pos:
                    original_hunger = agent.hunger
                    agent.hunger *= 0.5 # Reduce parent hunger
                    new_agent = PeacefulAgent(
                        self._get_next_id(),
                        birth_pos,
                        original_hunger * 0.5 * 0.6, # Give newborn fraction of parent's original reduced hunger
                        self.config,
                    )
                    agents_to_add.append(new_agent)
                    rewards[agent_id] = rewards.get(agent_id, 0) + self.config.REWARD_REPRODUCE
                    events.setdefault(agent_id, []).append("reproduced")
                    # Immediately occupy the cell on the map for the *next* step's calculations
                    self.shared_map[birth_pos] = PEACEFUL_CODE
                    # Add to agent dictionaries NOW so it's seen by subsequent checks in this step if needed
                    self._add_agent_to_dicts(new_agent)
                    # Add position to final lists for reward calculations if needed by others in this loop
                    final_peaceful_pos.add(birth_pos)

                    logging.debug(f"Agent {agent_id} reproduced. New agent {new_agent.id} at {birth_pos}")


        # --- 6. Cleanup Removed Agents & Spawn Food ---
        final_agents_removed_ids = set()
        for agent_id in agents_to_remove:
            # Agent object might already be gone if using _remove_agent inside loop, refetch if needed
            agent_to_remove_obj = self.agents.get(agent_id) # Get object to find its last known position
            pos_before_death = agent_to_remove_obj.pos if agent_to_remove_obj else None

            removed_agent = self._remove_agent(agent_id) # Official removal from dicts
            if removed_agent:
                final_agents_removed_ids.add(agent_id)
                # If agent died, ensure its final map square is cleared if no one else landed there
                if pos_before_death and self.shared_map[pos_before_death] == removed_agent.type_code:
                     # Check if another LIVE agent ended up here
                     occupied_by_live_agent = False
                     for live_id, live_agent in self.agents.items():
                          if live_agent.alive and live_agent.pos == pos_before_death:
                              occupied_by_live_agent = True
                              break
                     if not occupied_by_live_agent:
                           self.shared_map[pos_before_death] = self._get_terrain_code_from_base(*pos_before_death)
            # Ensure dones is set correctly
            dones[agent_id] = True


        # Add agents born this step (already added to dicts, ensure map is correct)
        # Their map squares should already be set during reproduction.

        self._spawn_food()

        # --- 7. Get Next States for Agents that Acted ---
        padded_map_after = self.get_padded_map() # Get map state AFTER all updates
        for agent_id in agent_ids_acted:
            if agent_id in final_agents_removed_ids or dones.get(agent_id, False): # Check if agent died this step
                next_states[agent_id] = None
                dones[agent_id] = True # Ensure done is True
            else:
                agent = self.agents.get(agent_id)
                if agent and agent.alive: # Should be alive if not in removed set
                    local_view_after = self.get_local_view(agent.pos, padded_map_after)
                    state_vector_after = agent.get_state_vector()
                    next_states[agent_id] = (local_view_after, state_vector_after)
                    dones[agent_id] = False # Ensure done is False if alive
                else:
                    # This case indicates a logic error somewhere if agent isn't removed but not found/alive
                    logging.error(f"Agent {agent_id} acted but is missing/dead unexpectedly at end of step.")
                    next_states[agent_id] = None
                    dones[agent_id] = True


        # Ensure returned dictionaries only contain keys for agents that acted initially
        final_rewards = {id: rewards.get(id, 0.0) for id in agent_ids_acted}
        final_dones = {id: dones.get(id, True) for id in agent_ids_acted} # Default to True if missing (error)

        return next_states, final_rewards, final_dones


# --- Simulation Worker (Corrected __init__, PER sampling check) ---
class SimulationWorker:
    """
    Рабочий класс симуляции, выполняющий основной цикл обучения и взаимодействия с окружением.

    Отвечает за:
    - Инициализацию моделей (основной и целевой) и оптимизаторов.
    - Инициализацию буферов воспроизведения опыта (PER).
    - Выполнение шагов симуляции: выбор действий агентов, взаимодействие с `Environment`.
    - Сбор опыта и сохранение его в буферы.
    - Обучение моделей на основе собранного опыта.
    - Периодическое обновление целевых моделей.
    - Управление состоянием симуляции (пауза, возобновление, остановка).
    - Сохранение и загрузку моделей.
    - Логирование процесса обучения и состояния симуляции.
    """
    def __init__(
        self, shared_map_ref: np.ndarray, config: Config, initial_user_map: np.ndarray
    ):
        """
        Инициализирует SimulationWorker.

        Args:
            shared_map_ref (np.ndarray): Ссылка на NumPy массив, который будет обновляться
                                         состоянием карты для визуализации.
            config (Config): Объект конфигурации.
            initial_user_map (np.ndarray): Начальная карта, предоставленная пользователем,
                                           для инициализации `Environment`.
        """
        self.config = config
        self.device = torch.device(config.DEVICE)
        logging.info(f"Worker using device: {self.device}")
        self.shared_map_ref = shared_map_ref
        self.initial_user_map = initial_user_map

        self.environment = Environment(self.initial_user_map, config)
        np.copyto(self.shared_map_ref, self.environment.shared_map)
        logging.debug(
            f"Environment created for worker. Initial map copied to shared reference."
        )

        common_nn_args = {
            "view_size": config.VIEW_SIZE,
            "state_dim": config.AGENT_STATE_DIM,
            "num_actions": config.NUM_ACTIONS,
            "num_object_codes": config.NUM_OBJECT_CODES,
            "embedding_dim": config.EMBEDDING_DIM,
        }
        try:
            self.peaceful_model = EmbeddingDuelingAgentNN(**common_nn_args).to(
                self.device
            )
            self.predator_model = EmbeddingDuelingAgentNN(**common_nn_args).to(
                self.device
            )
            self.peaceful_target_model = EmbeddingDuelingAgentNN(**common_nn_args).to(
                self.device
            )
            self.predator_target_model = EmbeddingDuelingAgentNN(**common_nn_args).to(
                self.device
            )

            self.peaceful_target_model.load_state_dict(self.peaceful_model.state_dict())
            self.predator_target_model.load_state_dict(self.predator_model.state_dict())
            self.peaceful_target_model.eval()
            self.predator_target_model.eval()

            self.peaceful_optimizer = optim.Adam(
                self.peaceful_model.parameters(), lr=config.LR_PEACEFUL
            )
            self.predator_optimizer = optim.Adam(
                self.predator_model.parameters(), lr=config.LR_PREDATOR
            )
            logging.debug(
                f"Models (EmbeddingDuelingAgentNN) created and optimizers initialized"
            )
        except Exception as e:
            logging.error(
                f"Failed to initialize models or optimizers: {e}", exc_info=True
            )
            raise

        # --- Инициализация PER Буферов ---
        buffer_capacity_per_type = config.BUFFER_SIZE // 2
        logging.info(
            f"Initializing PER buffers with capacity {buffer_capacity_per_type} each."
        )
        self.peaceful_buffer = PrioritizedReplayBuffer(
            buffer_capacity_per_type,
            config.PER_ALPHA,
            config.PER_BETA_START,
            config.PER_BETA_FRAMES,
            config.PER_EPSILON,
        )
        self.predator_buffer = PrioritizedReplayBuffer(
            buffer_capacity_per_type,
            config.PER_ALPHA,
            config.PER_BETA_START,
            config.PER_BETA_FRAMES,
            config.PER_EPSILON,
        )
        logging.debug(f"Prioritized Replay Buffers created")

        self._stop_event = threading.Event()
        self._pause_event = threading.Event()
        self.command_queue = queue.Queue()
        self.response_queue = queue.Queue()

        self.global_step = 0
        self.current_epsilon = config.EPS_START
        self.epoch_count = 0
        logging.debug("Worker __init__ completed successfully.")

    def _get_epsilon(self):
        """
        Рассчитывает текущее значение эпсилон для epsilon-greedy стратегии.

        Эпсилон линейно уменьшается от `EPS_START` до `EPS_END`
        за `EPS_DECAY_STEPS` шагов.

        Returns:
            float: Текущее значение эпсилон.
        """
        fraction = min(1.0, self.global_step / self.config.EPS_DECAY_STEPS)
        self.current_epsilon = self.config.EPS_START + fraction * (
            self.config.EPS_END - self.config.EPS_START
        )
        return self.current_epsilon

    def _select_action(
        self,
        agent_id: int,
        model: EmbeddingDuelingAgentNN,
        state: tuple[np.ndarray, np.ndarray],
    ) -> int:
        """
        Выбирает действие для агента на основе текущего состояния и epsilon-greedy стратегии.

        Args:
            agent_id (int): ID агента (для возможной отладки).
            model (EmbeddingDuelingAgentNN): Нейронная сеть, используемая для выбора действия.
            state (tuple[np.ndarray, np.ndarray]): Кортеж (локальный_вид, вектор_состояния) агента.

        Returns:
            int: Код выбранного действия.
        """
        if random.random() < self.current_epsilon:
            return random.randrange(self.config.NUM_ACTIONS)
        else:
            model.eval()
            with torch.no_grad():
                local_view_tensor = (
                    torch.from_numpy(state[0])
                    .unsqueeze(0)
                    .to(self.device, dtype=torch.long)
                )
                state_vec_tensor = (
                    torch.from_numpy(state[1])
                    .unsqueeze(0)
                    .to(self.device, dtype=torch.float32)
                )
                q_values = model(local_view_tensor, state_vec_tensor)
            model.train()
            return q_values.max(1)[1].item()

    def _preprocess_batch(self, batch: list[Experience]) -> Optional[tuple]:
        """
        Преобразует батч элементов `Experience` в тензоры PyTorch для обучения.

        Разделяет состояния, действия, вознаграждения, следующие состояния и флаги `done`.
        Особое внимание уделяется обработке `next_states`, так как они могут быть None
        для терминальных состояний.

        Args:
            batch (list[Experience]): Список элементов опыта.

        Returns:
            Optional[tuple]: Кортеж тензоров:
                ((state_views_t, state_vectors_t), actions_t, rewards_t,
                 (next_state_views_t, next_state_vectors_t), dones_mask, non_final_mask)
                Возвращает None, если батч пуст или произошла ошибка при обработке.
        """
        if not batch:
            return None
        try:
            states, actions, rewards, next_states_tuples, dones = zip(*batch)

            local_view_indices = np.array([s[0] for s in states], dtype=np.int64)
            state_vectors = np.array([s[1] for s in states], dtype=np.float32)
            state_views_t = torch.from_numpy(local_view_indices).to(
                self.device, dtype=torch.long
            )
            state_vectors_t = torch.from_numpy(state_vectors).to(
                self.device, dtype=torch.float32
            )

            non_final_mask_list = [s is not None for s in next_states_tuples]
            non_final_next_tuples = [s for s in next_states_tuples if s is not None]

            batch_size = len(batch)
            next_state_views_t = torch.zeros(
                batch_size,
                self.config.VIEW_SIZE,
                self.config.VIEW_SIZE,
                device=self.device,
                dtype=torch.long,
            )
            next_state_vectors_t = torch.zeros(
                batch_size,
                self.config.AGENT_STATE_DIM,
                device=self.device,
                dtype=torch.float32,
            )
            non_final_mask = torch.tensor(
                non_final_mask_list, device=self.device, dtype=torch.bool
            )

            if any(non_final_mask_list):
                non_final_next_views_list = [s[0] for s in non_final_next_tuples]
                non_final_next_vectors_list = [s[1] for s in non_final_next_tuples]

                non_final_next_views = np.array(
                    non_final_next_views_list, dtype=np.int64
                )
                non_final_next_vectors = np.array(
                    non_final_next_vectors_list, dtype=np.float32
                )
                next_views_tensor = torch.from_numpy(non_final_next_views).to(
                    self.device, dtype=torch.long
                )
                next_vectors_tensor = torch.from_numpy(non_final_next_vectors).to(
                    self.device, dtype=torch.float32
                )

                next_state_views_t[non_final_mask] = next_views_tensor
                next_state_vectors_t[non_final_mask] = next_vectors_tensor

            actions_t = torch.tensor(
                actions, device=self.device, dtype=torch.long
            ).unsqueeze(1)
            rewards_t = torch.tensor(
                rewards, device=self.device, dtype=torch.float32
            ).unsqueeze(1)
            dones_mask = torch.tensor(
                dones, device=self.device, dtype=torch.bool
            ).unsqueeze(1)

            return (
                (state_views_t, state_vectors_t),
                actions_t,
                rewards_t,
                (next_state_views_t, next_state_vectors_t),
                dones_mask,
                non_final_mask,
            )

        except Exception as e:
            logging.error(f"Error preprocessing batch: {e}", exc_info=True)
            # Log details about the problematic batch if possible
            # logging.error(f"Batch content (first item): {batch[0] if batch else 'Empty'}")
            return None

    def _train_step(
        self,
        model: EmbeddingDuelingAgentNN,
        target_model: EmbeddingDuelingAgentNN,
        buffer: PrioritizedReplayBuffer,
        optimizer: torch.optim.Optimizer,
    ):
        """
        Выполняет один шаг обучения модели с использованием PER и Double DQN.

        1. Семплирует батч из буфера с учетом приоритетов и IS-весов.
        2. Преобразует данные батча в тензоры.
        3. Рассчитывает текущие Q-значения Q(s,a) с помощью основной модели.
        4. Рассчитывает целевые Q-значения с помощью целевой модели и принципа Double DQN:
           Q_target(s,a) = r + gamma * Q_target_network(s', argmax_a' Q_main_network(s', a'))
           для нетерминальных состояний, и Q_target(s,a) = r для терминальных.
        5. Рассчитывает TD-ошибки для обновления приоритетов в буфере.
        6. Рассчитывает взвешенную (с IS-весами) Huber-потерю между текущими и целевыми Q-значениями.
        7. Выполняет шаг оптимизации (обратное распространение ошибки).
        8. Обновляет приоритеты в буфере.

        Args:
            model (EmbeddingDuelingAgentNN): Основная модель для обучения.
            target_model (EmbeddingDuelingAgentNN): Целевая модель для стабилизации обучения.
            buffer (PrioritizedReplayBuffer): Буфер воспроизведения опыта.
            optimizer (torch.optim.Optimizer): Оптимизатор для обновления весов модели.

        Returns:
            Optional[float]: Значение потерь на этом шаге обучения, или None, если обучение не состоялось
                             (например, буфер слишком мал или семплирование не удалось).
        """
        if len(buffer) < self.config.MIN_BUFFER_SIZE_FOR_TRAIN:
            # logging.debug(f"Skipping train step: Buffer size {len(buffer)} < {self.config.MIN_BUFFER_SIZE_FOR_TRAIN}")
            return None

        # 1. Sample batch with priorities
        sample_result = buffer.sample(self.config.BATCH_SIZE, self.global_step)
        if sample_result is None:
            # logging.debug("Skipping train step: PER sample returned None (buffer too small or sampling issue).")
            return None  # Buffer might be smaller than batch size or sampling failed
        batch, indices, is_weights = sample_result
        is_weights_t = torch.tensor(
            is_weights, device=self.device, dtype=torch.float32
        ).unsqueeze(1)

        # 2. Preprocess batch data into tensors
        preprocessed_data = self._preprocess_batch(batch)
        if preprocessed_data is None:
            logging.warning("Skipping train step: Batch preprocessing failed.")
            return None
        (
            states,
            actions,
            rewards,
            next_states,
            dones_mask,
            non_final_mask,
        ) = preprocessed_data
        state_views, state_vectors = states
        next_state_views, next_state_vectors = next_states

        # 3. Get current Q values from main model: Q(s, a)
        model.train()
        q_values = model(state_views, state_vectors)
        current_q_values = q_values.gather(1, actions)

        # 4. Get next Q values using Double DQN principle
        next_q_values_target = torch.zeros(
            self.config.BATCH_SIZE, 1, device=self.device, dtype=torch.float32
        )
        if non_final_mask.any():
            with torch.no_grad():
                nf_next_views = next_state_views[non_final_mask]
                nf_next_vectors = next_state_vectors[non_final_mask]

                # Ensure target model is in eval mode for stability
                target_model.eval()
                model.eval()  # Also eval main model for action selection consistency

                # Get Q values from main model for action selection
                next_q_values_main = model(nf_next_views, nf_next_vectors)
                best_next_actions = next_q_values_main.argmax(1).unsqueeze(1)

                # Get Q values for these best actions from the *target* network
                q_values_target_next = target_model(nf_next_views, nf_next_vectors)
                max_next_q = q_values_target_next.gather(1, best_next_actions)

                # Fill the tensor only for non_final states
                next_q_values_target[non_final_mask] = max_next_q

        # Switch models back to train mode if needed (model already set above)
        target_model.train()
        target_model.eval()
        model.train()

        # 5. Calculate target Q value: r + gamma * Q_target(s', argmax_a' Q(s', a')) * (1 - done)
        # Use the boolean `dones_mask` inverted for the (1 - done) part
        target_q_values = rewards + (
            self.config.GAMMA * next_q_values_target * (~dones_mask)
        )

        # 6. Calculate TD errors for priority updates (using detach)
        td_errors = (
            (target_q_values - current_q_values).detach().squeeze().cpu().numpy()
        )

        # 7. Calculate Huber loss (element-wise) weighted by IS weights
        loss_elementwise = F.smooth_l1_loss(
            current_q_values, target_q_values.detach(), reduction="none"
        )
        weighted_loss = (is_weights_t * loss_elementwise).mean()

        # 8. Optimization step
        optimizer.zero_grad()
        weighted_loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
        optimizer.step()

        # 9. Update priorities in the buffer
        buffer.update_priorities(indices, td_errors)

        return weighted_loss.item()

    def _update_target_networks(self):
        """
        Периодически обновляет веса целевых моделей (копирует веса из основных моделей).

        Частота обновления определяется `config.TARGET_UPDATE_FREQ`.
        """
        if (
            self.global_step > 0
            and self.global_step % self.config.TARGET_UPDATE_FREQ == 0
        ):
            logging.info(f"Step {self.global_step}: Updating target networks.")
            self.peaceful_target_model.load_state_dict(self.peaceful_model.state_dict())
            self.predator_target_model.load_state_dict(self.predator_model.state_dict())

            self.peaceful_target_model.eval()
            self.predator_target_model.eval()

    def run(self):
        """
        Основной цикл работы SimulationWorker.

        Включает в себя:
        - Обработку команд из очереди (пауза, возобновление, сохранение, остановка).
        - Проверку на вымирание видов и сброс среды при необходимости.
        - Выполнение шагов симуляции (выбор действий, взаимодействие с `Environment`).
        - Сбор опыта и его добавление в буферы.
        - Обучение моделей.
        - Обновление целевых сетей.
        - Обновление общей карты для визуализации (с ограничением частоты).
        - Логирование прогресса по эпохам.
        - Автоматическое сохранение моделей при завершении работы или ошибке.
        """
        logging.info("Worker run() started.")
        self.global_step = 0
        self.epoch_count = 0
        total_peaceful_reward_epoch = 0
        total_predator_reward_epoch = 0
        steps_this_epoch = 0
        peaceful_losses = []
        predator_losses = []

        last_map_update_time = time.time()
        map_update_interval = 1.0 / 30.0

        try:
            while not self._stop_event.is_set():
                # --- Handle Commands ---
                try:
                    command = self.command_queue.get_nowait()
                    if command == "pause":
                        self._pause_event.set()
                        self.response_queue.put("paused")
                        logging.info("Paused.")
                    elif command == "resume":
                        self._pause_event.clear()
                        self.response_queue.put("resumed")
                        logging.info("Resumed.")
                    elif isinstance(command, tuple) and command[0] == "save":
                        path = command[1]
                        self.save_models(path)
                        self.response_queue.put(
                            f"models_saved_step_{self.global_step}_epoch_{self.epoch_count}"
                        )
                    elif command == "get_epoch":
                        self.response_queue.put(self.epoch_count)
                    elif command == "get_step":
                        self.response_queue.put(self.global_step)
                    elif command == "stop":
                        self._stop_event.set()
                        self.response_queue.put("stopping")
                        logging.info("Stop command received.")
                        break
                except queue.Empty:
                    pass

                # --- Handle Pause ---
                if self._pause_event.is_set():
                    time.sleep(0.1)
                    continue

                # --- CHECK FOR EXTINCTION AND RESET ---
                peaceful_count = len(self.environment.peaceful_agents)
                predator_count = len(self.environment.predator_agents)
                if self.global_step > 100 and (
                    peaceful_count == 0 or predator_count == 0
                ):
                    extinct_type = "Peaceful" if peaceful_count == 0 else "Predator"
                    logging.warning(
                        f"!!! {extinct_type} agents died out (P:{peaceful_count}, R:{predator_count}) at step {self.global_step}! Resetting environment and clearing buffers. !!!"
                    )
                    self.environment.reset()

                    np.copyto(self.shared_map_ref, self.environment.shared_map)
                    self.peaceful_buffer.clear()
                    self.predator_buffer.clear()

                    total_peaceful_reward_epoch = 0
                    total_predator_reward_epoch = 0
                    steps_this_epoch = 0
                    peaceful_losses = []
                    predator_losses = []
                    logging.warning(
                        "!!! Environment reset complete. Continuing simulation. !!!"
                    )
                    peaceful_count = len(self.environment.peaceful_agents)
                    predator_count = len(self.environment.predator_agents)
                    continue

                # --- Simulation Step ---
                actions_to_take = {}
                current_states = {}
                agent_types_before_step = {}

                active_agent_ids = list(self.environment.agents.keys())
                if not active_agent_ids:
                    # logging.debug("No active agents in environment. Waiting...")
                    time.sleep(0.05)
                    continue

                # 1. Get states, types, and select actions for ALIVE agents
                valid_agents_for_step = 0
                for agent_id in active_agent_ids:
                    agent = self.environment.agents.get(agent_id)

                    if not agent or not agent.alive:
                        continue

                    state_tuple = self.environment.get_agent_state(agent_id)
                    if state_tuple is None:
                        logging.warning(
                            f"Could not get state for alive agent {agent_id}. Skipping."
                        )
                        continue

                    current_states[agent_id] = state_tuple
                    agent_types_before_step[agent_id] = agent.type_code

                    model = (
                        self.peaceful_model
                        if agent.type_code == PEACEFUL_CODE
                        else self.predator_model
                    )
                    action = self._select_action(agent_id, model, state_tuple)
                    actions_to_take[agent_id] = action
                    valid_agents_for_step += 1

                if not actions_to_take:
                    # logging.debug("No valid actions selected for any agent. Waiting...")
                    time.sleep(0.05)
                    continue

                # 2. Execute step in environment
                next_states, rewards, dones = self.environment.step(actions_to_take)

                # 3. Store experience in PER buffers
                step_peaceful_reward = 0
                step_predator_reward = 0

                for agent_id in actions_to_take.keys():
                    state = current_states.get(agent_id)
                    action = actions_to_take.get(agent_id)
                    reward = rewards.get(agent_id)
                    next_state = next_states.get(agent_id)
                    done = dones.get(agent_id)
                    agent_type = agent_types_before_step.get(agent_id)

                    if (
                        state is not None
                        and action is not None
                        and reward is not None
                        and done is not None
                        and agent_type is not None
                    ):
                        buffer_to_use = None
                        if agent_type == PEACEFUL_CODE:
                            buffer_to_use = self.peaceful_buffer
                            step_peaceful_reward += reward
                        elif agent_type == PREDATOR_CODE:
                            buffer_to_use = self.predator_buffer
                            step_predator_reward += reward

                        if buffer_to_use is not None:
                            buffer_to_use.push(
                                None, state, action, reward, next_state, done
                            )

                total_peaceful_reward_epoch += step_peaceful_reward
                total_predator_reward_epoch += step_predator_reward
                steps_this_epoch += 1

                # 4. Perform training steps (if freq met and buffer large enough)
                peaceful_loss, predator_loss = None, None
                if (
                    self.global_step > self.config.MIN_BUFFER_SIZE_FOR_TRAIN
                    and self.global_step % self.config.TRAIN_FREQ == 0
                ):
                    peaceful_loss = self._train_step(
                        self.peaceful_model,
                        self.peaceful_target_model,
                        self.peaceful_buffer,
                        self.peaceful_optimizer,
                    )
                    if peaceful_loss is not None:
                        peaceful_losses.append(peaceful_loss)

                    predator_loss = self._train_step(
                        self.predator_model,
                        self.predator_target_model,
                        self.predator_buffer,
                        self.predator_optimizer,
                    )
                    if predator_loss is not None:
                        predator_losses.append(predator_loss)

                # 5. Update target networks periodically
                self._update_target_networks()

                # 6. Update the shared map reference for visualization (throttled)
                current_time = time.time()
                if current_time - last_map_update_time >= map_update_interval:
                    np.copyto(self.shared_map_ref, self.environment.shared_map)
                    last_map_update_time = current_time

                # 7. Update counters and log epoch info
                self.global_step += 1
                if (
                    self.global_step > 0
                    and self.global_step % self.config.EPOCH_LENGTH_STEPS == 0
                ):
                    self.epoch_count += 1
                    avg_steps_for_reward = max(1, steps_this_epoch)

                    peaceful_count_now = len(self.environment.peaceful_agents)
                    predator_count_now = len(self.environment.predator_agents)
                    food_count_now = len(self.environment.food_locations)

                    avg_peaceful_reward = (
                        total_peaceful_reward_epoch / avg_steps_for_reward
                    )
                    avg_predator_reward = (
                        total_predator_reward_epoch / avg_steps_for_reward
                    )
                    avg_peaceful_loss = sum(peaceful_losses) / max(
                        1, len(peaceful_losses)
                    )
                    avg_predator_loss = sum(predator_losses) / max(
                        1, len(predator_losses)
                    )

                    logging.info(
                        f"Epoch {self.epoch_count} (Step {self.global_step}): "
                        f"Eps: {self.current_epsilon:.3f} | "
                        f"P Rwd: {avg_peaceful_reward:.3f}, Loss: {avg_peaceful_loss:.4f}, Cnt: {peaceful_count_now}, Buf: {len(self.peaceful_buffer)} | "
                        f"R Rwd: {avg_predator_reward:.3f}, Loss: {avg_predator_loss:.4f}, Cnt: {predator_count_now}, Buf: {len(self.predator_buffer)} | "
                        f"Food: {food_count_now}"
                    )
                    total_peaceful_reward_epoch = 0
                    total_predator_reward_epoch = 0
                    steps_this_epoch = 0
                    peaceful_losses = []
                    predator_losses = []

        except Exception as e:
            logging.error(
                f"!!! CRITICAL ERROR in worker run loop at step {self.global_step}: {e} !!!",
                exc_info=True,
            )
        finally:
            logging.warning(
                f"Worker run loop finished or encountered error at step {self.global_step}. Saving final models..."
            )
            self.save_models(self.config.SAVE_MODEL_DIR)
            logging.info("Simulation worker stopping process complete.")

    def save_models(self, path="."):
        """
        Сохраняет текущие состояния основных моделей (мирной и хищной) в указанную директорию.

        Модели сначала перемещаются на CPU для сохранения, чтобы избежать проблем
        с совместимостью устройств при загрузке.

        Args:
            path (str, optional): Директория для сохранения моделей.
                                  По умолчанию используется `self.config.SAVE_MODEL_DIR`.
        """
        try:
            os.makedirs(path, exist_ok=True)
            peaceful_path = os.path.join(path, self.config.PEACEFUL_MODEL_FILENAME)
            predator_path = os.path.join(path, self.config.PREDATOR_MODEL_FILENAME)

            self.peaceful_model.cpu()
            self.predator_model.cpu()
            torch.save(self.peaceful_model.state_dict(), peaceful_path)
            torch.save(self.predator_model.state_dict(), predator_path)

            self.peaceful_model.to(self.device)
            self.predator_model.to(self.device)
            logging.info(f"Models saved to {peaceful_path} and {predator_path}")
        except Exception as e:
            logging.error(f"Failed to save models to {path}: {e}", exc_info=True)

    def load_models(self, path="."):
        """
        Загружает веса для основных и целевых моделей из указанной директории.

        Если файлы моделей не найдены, инициализирует модели случайными весами.
        После загрузки модели перемещаются на настроенное устройство (`self.device`)
        и устанавливаются в соответствующие режимы (train/eval).

        Args:
            path (str, optional): Директория, из которой загружаются модели.
                                  По умолчанию используется `self.config.SAVE_MODEL_DIR`.

        Returns:
            Tuple[bool, bool]: Кортеж (peaceful_loaded, predator_loaded), где True означает
                               успешную загрузку соответствующей модели.
        """
        loaded_peaceful = False
        loaded_predator = False
        try:
            peaceful_path = os.path.join(path, self.config.PEACEFUL_MODEL_FILENAME)
            predator_path = os.path.join(path, self.config.PREDATOR_MODEL_FILENAME)

            if os.path.exists(peaceful_path):
                state_dict_p = torch.load(peaceful_path, map_location=self.device)
                self.peaceful_model.load_state_dict(state_dict_p)
                self.peaceful_target_model.load_state_dict(state_dict_p)
                logging.info(f"Loaded peaceful model from {peaceful_path}")
                loaded_peaceful = True
            else:
                logging.warning(
                    f"Peaceful model file not found: {peaceful_path}. Starting with random weights."
                )

            if os.path.exists(predator_path):
                state_dict_r = torch.load(predator_path, map_location=self.device)
                self.predator_model.load_state_dict(state_dict_r)
                self.predator_target_model.load_state_dict(state_dict_r)
                logging.info(f"Loaded predator model from {predator_path}")
                loaded_predator = True
            else:
                logging.warning(
                    f"Predator model file not found: {predator_path}. Starting with random weights."
                )

            # Set models to appropriate modes after loading
            self.peaceful_model.to(self.device)
            self.predator_model.to(self.device)
            self.peaceful_target_model.to(self.device)
            self.predator_target_model.to(self.device)

            self.peaceful_model.train()
            self.predator_model.train()
            self.peaceful_target_model.eval()
            self.predator_target_model.eval()
            logging.info(
                "Models set to appropriate train/eval modes after loading/initialization."
            )

        except FileNotFoundError as e:
            logging.warning(f"Model file not found during load: {e}. Starting fresh.")
        except Exception as e:
            logging.error(f"Failed to load models from {path}: {e}", exc_info=True)
            logging.warning("Proceeding with random weights due to load error.")
            self.peaceful_model.to(self.device).train()
            self.predator_model.to(self.device).train()
            self.peaceful_target_model.to(self.device).eval()
            self.predator_target_model.to(self.device).eval()

        return loaded_peaceful, loaded_predator


# --- Simulation API (Corrected __init__, added get_step) ---
class SimulationAPI:
    """
    API для управления симуляцией.

    Предоставляет интерфейс для запуска, остановки, приостановки и возобновления
    симуляции, а также для сохранения моделей и получения информации о состоянии
    симуляции (например, текущая эпоха, шаг, снимок карты).
    Работает с `SimulationWorker` в отдельном потоке.
    """
    def __init__(self, initial_map: np.ndarray, config: Config):
        """
        Инициализирует Simulation API.

        Args:
            initial_map (np.ndarray): NumPy массив, представляющий начальное состояние мира.
                                      Должен содержать коды местности и размещенных сущностей.
                                      Ожидаемые коды: SURFACE_CODE, WATER_CODE, OBSTACLE_CODE,
                                      PEACEFUL_CODE, PREDATOR_CODE, FOOD_CODE.
                                      Размерность должна соответствовать MAP_HEIGHT, MAP_WIDTH из конфига.
            config (Config): Объект конфигурации `Config`.
        """
        if not isinstance(initial_map, np.ndarray):
            raise ValueError("initial_map must be a NumPy array.")
        if initial_map.shape != (config.MAP_HEIGHT, config.MAP_WIDTH):
            raise ValueError(
                f"Map shape mismatch: Expected ({config.MAP_HEIGHT}, {config.MAP_WIDTH}), Got {initial_map.shape}"
            )
        unique_codes = np.unique(initial_map)
        if not all(0 <= code < config.NUM_OBJECT_CODES for code in unique_codes):
            logging.warning(
                f"Initial map contains unexpected codes: {unique_codes}. Ensure codes are within [0, {config.NUM_OBJECT_CODES-1}]"
            )
        if initial_map.dtype not in [np.int8, np.int32, np.int64]:
            logging.warning(
                f"Initial map dtype is {initial_map.dtype}. Forcing to int8."
            )
            initial_map = initial_map.astype(np.int8)

        self.config = config
        self.initial_user_map = initial_map.copy()
        self.shared_map_for_viz = np.zeros_like(initial_map, dtype=initial_map.dtype)

        self.worker: Optional[SimulationWorker] = None
        self.worker_thread: Optional[threading.Thread] = None
        logging.debug("SimulationAPI initialized.")

    def start(self):
        """
        Запускает SimulationWorker в отдельном потоке.

        Перед запуском потока пытается загрузить сохраненные модели.
        Если симуляция уже запущена, выводит предупреждение.
        """
        if self.is_running():
            logging.warning("Simulation worker is already running.")
            return

        # 1. Create worker, passing the shared map ref and the initial map
        try:
            self.worker = SimulationWorker(
                self.shared_map_for_viz, self.config, self.initial_user_map
            )
        except Exception as e:
            logging.error(f"Failed to create SimulationWorker: {e}", exc_info=True)
            self.worker = None
            return

        # 2. Attempt to load weights BEFORE starting the thread
        logging.info(
            f"Attempting to load models from default directory: {self.config.SAVE_MODEL_DIR}"
        )
        loaded_p, loaded_r = self.worker.load_models(self.config.SAVE_MODEL_DIR)
        if not loaded_p and not loaded_r:
            logging.info(
                "No existing models found or loaded. Starting training from scratch."
            )
        else:
            logging.info(f"Models loaded: Peaceful={loaded_p}, Predator={loaded_r}")

        # 3. Start the worker thread
        self.worker_thread = threading.Thread(
            target=self.worker.run, daemon=True, name="SimulationWorkerThread"
        )
        self.worker_thread.start()
        logging.info("Simulation worker thread started.")

    def _send_command(self, command, wait_for_response=True, timeout=5.0):
        """
        Отправляет команду в очередь команд SimulationWorker.

        Args:
            command (any): Команда для отправки (строка или кортеж).
            wait_for_response (bool, optional): Ожидать ли ответ от воркера. По умолчанию True.
            timeout (float, optional): Максимальное время ожидания ответа в секундах. По умолчанию 5.0.

        Returns:
            Optional[any]: Ответ от воркера, если `wait_for_response`=True и ответ получен.
                           "command_sent", если `wait_for_response`=False и команда успешно отправлена.
                           None в случае ошибки или таймаута.
        """
        if not self.is_running() or self.worker is None:
            logging.error("Cannot send command: Worker is not running.")
            return None
        try:
            self.worker.command_queue.put(command)
            if wait_for_response:
                try:
                    return self.worker.response_queue.get(timeout=timeout)
                except queue.Empty:
                    logging.warning(
                        f"Timeout waiting for response for command '{command}'"
                    )
                    return None
            return "command_sent"
        except Exception as e:
            logging.error(f"Error sending command '{command}': {e}", exc_info=True)
            return None

    def pause(self):
        """
        Отправляет команду приостановки симуляции.

        Returns:
            Optional[str]: "paused" в случае успеха, None в случае ошибки или таймаута.
        """
        return self._send_command("pause")

    def resume(self):
        """
        Отправляет команду возобновления симуляции.

        Returns:
            Optional[str]: "resumed" в случае успеха, None в случае ошибки или таймаута.
        """
        return self._send_command("resume")

    def save_models(self, path=None):
        """
        Отправляет команду сохранения моделей.

        Args:
            path (Optional[str], optional): Путь для сохранения моделей.
                                            Если None, используется путь из конфига.

        Returns:
            Optional[str]: Строка с информацией о сохранении от воркера, или None.
        """
        save_dir = path if path is not None else self.config.SAVE_MODEL_DIR
        logging.info(f"API: Requesting model save to '{save_dir}'...")
        return self._send_command(("save", save_dir))

    def get_epoch(self) -> Optional[int]:
        """
        Запрашивает текущий номер эпохи у воркера.

        Returns:
            Optional[int]: Текущий номер эпохи, или None в случае ошибки.
        """
        response = self._send_command("get_epoch")
        return response if isinstance(response, int) else None

    def get_step(self) -> Optional[int]:
        """
        Запрашивает текущий глобальный шаг симуляции у воркера.

        Returns:
            Optional[int]: Текущий глобальный шаг, или None в случае ошибки.
        """
        response = self._send_command("get_step")
        return response if isinstance(response, int) else None

    def stop(self, timeout=10.0):
        """
        Останавливает SimulationWorker и ожидает завершения его потока.

        Args:
            timeout (float, optional): Максимальное время ожидания завершения потока воркера
                                       в секундах. По умолчанию 10.0.

        Returns:
            Optional[str]: Ответ от воркера на команду остановки, или None.
        """
        logging.info("API: Requesting worker stop...")
        response = self._send_command(
            "stop", wait_for_response=True, timeout=max(1.0, timeout - 1)
        )
        if self.worker_thread:
            logging.info(f"API: Waiting up to {timeout}s for worker thread to join...")
            self.worker_thread.join(timeout=timeout)
            if self.worker_thread.is_alive():
                logging.warning("Worker thread did not stop gracefully within timeout.")
            else:
                logging.info("Worker thread stopped.")
            self.worker_thread = None
        else:
            logging.debug("API: Worker thread was already None.")
        self.worker = None
        logging.info(f"Stop command response from worker: {response}")
        return response

    def get_map_snapshot(self) -> Optional[np.ndarray]:
        """
        Возвращает копию текущего состояния общей карты (shared_map_for_viz).

        Карта обновляется воркером и предназначена для визуализации.

        Returns:
            Optional[np.ndarray]: Копия карты, или None, если карта не инициализирована.
        """
        if self.shared_map_for_viz is not None:
            return self.shared_map_for_viz.copy()
        else:
            logging.error("Cannot get map snapshot, shared_map_for_viz is None.")
            return None

    def is_running(self) -> bool:
        """
        Проверяет, запущен ли и активен ли поток SimulationWorker.

        Returns:
            bool: True, если воркер запущен и работает, иначе False.
        """
        return self.worker_thread is not None and self.worker_thread.is_alive()
