import pytest
import numpy as np
import torch
import torch.nn as nn
import os
import queue
import random
import time
import shutil
from unittest.mock import patch, MagicMock

random.seed(23523555)
from ai_life_simulator._backend.agents import (
    Config,
    Agent,
    PeacefulAgent,
    PredatorAgent,
    Environment,
    SumTree,
    PrioritizedReplayBuffer,
    Experience,
    EmbeddingDuelingAgentNN,
    SimulationWorker,
    SimulationAPI,
    SURFACE_CODE,
    PEACEFUL_CODE,
    PREDATOR_CODE,
    FOOD_CODE,
    WATER_CODE,
    OBSTACLE_CODE,
)


@pytest.fixture
def config():
    """Фикстура для создания объекта конфигурации `Config` с тестовыми параметрами."""
    cfg = Config(
        MAP_WIDTH=10,
        MAP_HEIGHT=8,
        VIEW_SIZE=3,
        BUFFER_SIZE=1000,
        BATCH_SIZE=8,
        INITIAL_PEACEFUL=2,
        INITIAL_PREDATORS=1,
        INITIAL_FOOD=5,
        MAX_FOOD=10,
        EPOCH_LENGTH_STEPS=50,
        MIN_BUFFER_SIZE_FOR_TRAIN=10,
        TARGET_UPDATE_FREQ=20,
        TRAIN_FREQ=2,
        PER_BETA_FRAMES=100,
        EPS_DECAY_STEPS=100,
        REWARD_STEP=-0.01,
        REWARD_EAT_FOOD=20.0,
        REWARD_EAT_PREY=50.0,
        REWARD_DEATH=-100.0,
        HUNGER_DECAY_STEP=0.1,
        MAX_HUNGER=100.0,
        REPRODUCTION_THRESHOLD=95.0,
        REWARD_APPROACH_FOOD_FACTOR=0.5,
        REWARD_FLEE_PREDATOR_FACTOR=0.6,
        REWARD_APPROACH_PREY_FACTOR=0.7,
        DEVICE="cpu",
    )
    return cfg


@pytest.fixture
def simple_map(config):
    """Фикстура для создания простой тестовой карты (numpy array)."""
    map_ = np.full((config.MAP_HEIGHT, config.MAP_WIDTH), SURFACE_CODE, dtype=np.int8)
    map_[0, 0] = OBSTACLE_CODE
    map_[1, 1] = WATER_CODE
    map_[2, 2] = FOOD_CODE
    map_[3, 3] = PEACEFUL_CODE
    map_[4, 4] = PREDATOR_CODE
    return map_


@pytest.fixture
def env(simple_map, config):
    """Фикстура для создания объекта окружения `Environment` на основе простой карты."""
    return Environment(simple_map, config)


@pytest.fixture
def agent(config):
    """Фикстура для создания базового объекта агента `Agent`."""
    return Agent(id=1, pos=(1, 1), hunger=50.0, type_code=PEACEFUL_CODE, config=config)


@pytest.fixture
def peaceful_agent(config):
    """Фикстура для создания мирного агента `PeacefulAgent`."""
    return PeacefulAgent(id=1, pos=(3, 3), hunger=80.0, config=config)


@pytest.fixture
def predator_agent(config):
    """Фикстура для создания хищного агента `PredatorAgent`."""
    return PredatorAgent(id=2, pos=(4, 4), hunger=70.0, config=config)


@pytest.fixture
def sum_tree():
    """Фикстура для создания объекта `SumTree` (дерево сумм)."""
    return SumTree(capacity=16)


@pytest.fixture
def per_buffer(config):
    """Фикстура для создания буфера с приоритезированным воспроизведением опыта (`PrioritizedReplayBuffer`)."""
    return PrioritizedReplayBuffer(
        capacity=config.BUFFER_SIZE // 2,
        alpha=config.PER_ALPHA,
        beta_start=config.PER_BETA_START,
        beta_frames=config.PER_BETA_FRAMES,
        epsilon=config.PER_EPSILON,
    )


@pytest.fixture
def dummy_model(config):
    """Фикстура для создания 'пустышки' модели нейронной сети `EmbeddingDuelingAgentNN`."""
    model = EmbeddingDuelingAgentNN(
        view_size=config.VIEW_SIZE,
        state_dim=config.AGENT_STATE_DIM,
        num_actions=config.NUM_ACTIONS,
        num_object_codes=config.NUM_OBJECT_CODES,
        embedding_dim=config.EMBEDDING_DIM,
    ).to(config.DEVICE)
    return model


@pytest.fixture
def dummy_experience(config):
    """Фикстура для создания объекта 'опыта' `Experience` со случайными данными."""
    view = np.random.randint(
        0,
        config.NUM_OBJECT_CODES,
        size=(config.VIEW_SIZE, config.VIEW_SIZE),
        dtype=np.int64,
    )
    state_vec = np.random.rand(config.AGENT_STATE_DIM).astype(np.float32)
    action = np.random.randint(0, config.NUM_ACTIONS)
    reward = np.random.rand()
    next_view = np.random.randint(
        0,
        config.NUM_OBJECT_CODES,
        size=(config.VIEW_SIZE, config.VIEW_SIZE),
        dtype=np.int64,
    )
    next_state_vec = np.random.rand(config.AGENT_STATE_DIM).astype(np.float32)
    done = False
    return Experience(
        state=(view, state_vec),
        action=action,
        reward=reward,
        next_state=(next_view, next_state_vec),
        done=done,
    )


@pytest.fixture
def simulation_worker(simple_map, config):
    """Фикстура для создания объекта `SimulationWorker`."""
    shared_map_ref = simple_map.copy()  # Создаем копию карты для воркера
    try:
        worker = SimulationWorker(shared_map_ref, config, simple_map)
    except Exception as e:
        pytest.fail(f"Failed to initialize SimulationWorker: {e}")
    return worker


@pytest.fixture
def simulation_api(simple_map, config, tmp_path):
    """Фикстура для создания объекта `SimulationAPI` для управления симуляцией."""
    config.SAVE_MODEL_DIR = str(
        tmp_path / "saved_models"
    )  # Используем временную директорию для моделей
    api = SimulationAPI(simple_map, config)
    yield api
    if api.is_running():  # Гарантируем остановку API после теста
        api.stop(timeout=1.0)


class TestConfig:
    """Тесты для класса конфигурации `Config`."""

    def test_defaults(self):
        """Тестирует значения по умолчанию в конфигурации."""
        cfg = Config()
        assert cfg.MAP_WIDTH == 60
        assert cfg.PAD_VALUE == OBSTACLE_CODE
        assert cfg.DEVICE in [
            "cuda",
            "cpu",
        ]  # Проверяем, что устройство выбрано корректно


class TestAgent:
    """Тесты для базового класса агента `Agent`."""

    def test_init(self, agent, config):
        """Тестирует инициализацию агента."""
        assert agent.id == 1
        assert agent.pos == (1, 1)
        assert agent.hunger == 50.0
        assert agent.alive is True
        assert agent.config == config

    def test_normalize_hunger(self, agent):
        """Тестирует нормализацию значения голода агента (от 0 до 1)."""
        agent.hunger = 75.0
        assert agent.normalize_hunger() == 0.75
        agent.hunger = 120.0  # Голод выше максимального
        assert agent.normalize_hunger() == 1.0
        agent.hunger = -10.0  # Голод ниже минимального (не должно происходить в норме)
        assert agent.normalize_hunger() == 0.0

    def test_get_state_vector(self, agent, config):
        """Тестирует получение вектора состояния агента (например, нормализованный голод)."""
        agent.hunger = 50.0
        state_vec = agent.get_state_vector()
        assert isinstance(state_vec, np.ndarray)
        assert state_vec.shape == (config.AGENT_STATE_DIM,)
        assert state_vec.dtype == np.float32
        np.testing.assert_array_almost_equal(
            state_vec, [0.5]
        )  # Нормализованный голод 50/100 = 0.5

    def test_peaceful_predator_init(self, peaceful_agent, predator_agent, config):
        """Тестирует инициализацию мирного и хищного агентов, проверяя их типы."""
        assert peaceful_agent.type_code == PEACEFUL_CODE
        assert predator_agent.type_code == PREDATOR_CODE
        assert peaceful_agent.config == config
        assert predator_agent.config == config


class TestSumTree:
    """Тесты для структуры данных `SumTree` (дерево сумм)."""

    def test_init(self, sum_tree):
        """Тестирует инициализацию `SumTree`."""
        assert sum_tree.capacity == 16
        assert len(sum_tree.tree) == 2 * 16 - 1  # Размер внутреннего массива дерева
        assert len(sum_tree.data) == 16  # Размер массива для хранения данных
        assert sum_tree.data_size == 0  # Начальное количество элементов

    def test_add_and_total(self, sum_tree):
        """Тестирует добавление элементов и подсчет общей суммы приоритетов."""
        sum_tree.add(10.0, "data1")
        sum_tree.add(5.0, "data2")
        assert sum_tree.data_size == 2
        assert sum_tree.total() == pytest.approx(15.0)
        assert sum_tree.data[0] == "data1"
        assert sum_tree.data[1] == "data2"
        # Проверяем, что приоритеты корректно записаны в листья дерева
        assert sum_tree.tree[sum_tree.capacity - 1 + 0] == pytest.approx(10.0)
        assert sum_tree.tree[sum_tree.capacity - 1 + 1] == pytest.approx(5.0)

    def test_update(self, sum_tree):
        """Тестирует обновление приоритета элемента в дереве."""
        sum_tree.add(10.0, "data1")
        sum_tree.add(5.0, "data2")
        tree_idx = (
            sum_tree.capacity - 1 + 0
        )  # Индекс первого добавленного элемента в массиве tree
        sum_tree.update(tree_idx, 20.0)
        assert sum_tree.tree[tree_idx] == pytest.approx(20.0)
        assert sum_tree.total() == pytest.approx(25.0)  # Общая сумма должна измениться

    def test_get_leaf(self, sum_tree):
        """Тестирует получение элемента (листа) из дерева по значению (сэмплирование)."""
        sum_tree.add(10.0, "data1")
        sum_tree.add(5.0, "data2")
        sum_tree.add(15.0, "data3")  # Общая сумма = 30
        # Сэмплируем значения в разных диапазонах
        idx, p, data = sum_tree.get_leaf(5.0)  # Должен попасть в data1 (приоритет 10)
        assert data == "data1"
        assert p == pytest.approx(10.0)
        assert idx == sum_tree.capacity - 1 + 0

        idx, p, data = sum_tree.get_leaf(
            12.0
        )  # Должен попасть в data2 (диапазон 10-15, приоритет 5)
        assert data == "data2"
        assert p == pytest.approx(5.0)
        assert idx == sum_tree.capacity - 1 + 1

        idx, p, data = sum_tree.get_leaf(
            25.0
        )  # Должен попасть в data3 (диапазон 15-30, приоритет 15)
        assert data == "data3"
        assert p == pytest.approx(15.0)
        assert idx == sum_tree.capacity - 1 + 2

    def test_len(self, sum_tree):
        """Тестирует получение количества элементов в дереве."""
        assert len(sum_tree) == 0
        sum_tree.add(1.0, "d1")
        assert len(sum_tree) == 1
        for i in range(20):  # Добавляем больше элементов, чем вместимость
            sum_tree.add(float(i), f"d{i+2}")
        assert (
            len(sum_tree) == sum_tree.capacity
        )  # Размер не должен превышать вместимость


class TestPrioritizedReplayBuffer:
    """Тесты для буфера с приоритезированным воспроизведением опыта (`PrioritizedReplayBuffer`)."""

    def test_init(self, per_buffer):
        """Тестирует инициализацию `PrioritizedReplayBuffer`."""
        assert per_buffer.capacity > 0
        assert isinstance(per_buffer.tree, SumTree)

    def test_push(self, per_buffer, dummy_experience):
        """Тестирует добавление опыта в буфер."""
        state, action, reward, next_state, done = dummy_experience
        # Добавление с None ошибкой (максимальный приоритет)
        per_buffer.push(None, state, action, reward, next_state, done)
        assert len(per_buffer) == 1
        assert per_buffer.tree.total() == per_buffer._max_priority

        # Добавление с конкретной ошибкой
        error = 10.0
        priority = (abs(error) + per_buffer.epsilon) ** per_buffer.alpha
        per_buffer.push(error, state, action, reward, next_state, done)
        assert len(per_buffer) == 2
        assert per_buffer.tree.total() == pytest.approx(
            per_buffer._max_priority + priority
        )

    def test_sample(self, per_buffer, dummy_experience, config):
        """Тестирует выборку (сэмплирование) опыта из буфера."""
        # Попытка выборки из пустого/недостаточно заполненного буфера
        assert per_buffer.sample(config.BATCH_SIZE, 0) is None
        state, action, reward, next_state, done = dummy_experience
        # Заполняем буфер достаточным количеством элементов
        for i in range(config.BATCH_SIZE * 2):
            per_buffer.push(
                abs(i * 0.1) + 0.1, state, action, reward + i, next_state, done
            )
        assert len(per_buffer) >= config.BATCH_SIZE

        sample_result = per_buffer.sample(
            config.BATCH_SIZE, 10
        )  # frame_num = 10 для расчета beta
        assert sample_result is not None
        batch, indices, is_weights = sample_result
        assert len(batch) == config.BATCH_SIZE
        assert len(indices) == config.BATCH_SIZE
        assert len(is_weights) == config.BATCH_SIZE
        assert all(isinstance(exp, Experience) for exp in batch)
        assert all(
            idx >= per_buffer.tree.capacity - 1 for idx in indices
        )  # Индексы должны быть из листьев дерева
        assert all(
            0.0 <= w <= 1.0 + 1e-6 for w in is_weights
        )  # Веса IS должны быть в разумных пределах

    def test_update_priorities(self, per_buffer, dummy_experience, config):
        """Тестирует обновление приоритетов для выбранных элементов опыта."""
        state, action, reward, next_state, done = dummy_experience
        # Заполняем буфер
        for i in range(config.BATCH_SIZE + 5):
            per_buffer.push(i + 1.0, state, action, reward + i, next_state, done)

        sample_result = per_buffer.sample(config.BATCH_SIZE, 0)
        assert sample_result is not None
        _, indices, _ = sample_result
        new_errors = (
            np.random.rand(config.BATCH_SIZE) * 5.0 + 0.1
        )  # Генерируем новые ошибки
        per_buffer.update_priorities(indices, new_errors)

        # Проверяем, что приоритет одного из элементов обновлен корректно
        idx_to_check = indices[0]
        expected_new_p = (abs(new_errors[0]) + per_buffer.epsilon) ** per_buffer.alpha
        assert per_buffer.tree.tree[idx_to_check] == pytest.approx(expected_new_p)

    def test_clear(self, per_buffer, dummy_experience):
        """Тестирует очистку буфера."""
        state, action, reward, next_state, done = dummy_experience
        per_buffer.push(None, state, action, reward, next_state, done)
        assert len(per_buffer) == 1
        per_buffer.clear()
        assert len(per_buffer) == 0
        assert per_buffer.tree.total() == 0.0
        assert (
            per_buffer._max_priority == 1.0
        )  # _max_priority сбрасывается к значению по умолчанию


class TestEmbeddingDuelingAgentNN:
    """Тесты для нейронной сети `EmbeddingDuelingAgentNN`."""

    def test_init(self, dummy_model, config):
        """Тестирует инициализацию слоев нейронной сети."""
        assert isinstance(dummy_model.embedding, nn.Embedding)
        assert dummy_model.embedding.num_embeddings == config.NUM_OBJECT_CODES
        assert dummy_model.embedding.embedding_dim == config.EMBEDDING_DIM
        assert isinstance(dummy_model.fc_value, nn.Linear)  # Слой для Value stream
        assert isinstance(
            dummy_model.fc_advantage, nn.Linear
        )  # Слой для Advantage stream

    def test_forward_pass(self, dummy_model, config):
        """Тестирует прямой проход батча данных через сеть в режиме обучения."""
        batch_size = 4
        view_indices = torch.randint(
            0,
            config.NUM_OBJECT_CODES,
            (batch_size, config.VIEW_SIZE, config.VIEW_SIZE),
            dtype=torch.long,
        ).to(config.DEVICE)
        agent_state = torch.randn(
            batch_size, config.AGENT_STATE_DIM, dtype=torch.float32
        ).to(config.DEVICE)

        dummy_model.train()  # Устанавливаем режим обучения
        q_values = dummy_model(view_indices, agent_state)

        assert isinstance(q_values, torch.Tensor)
        assert q_values.shape == (batch_size, config.NUM_ACTIONS)
        assert q_values.device.type == config.DEVICE
        assert not torch.isnan(q_values).any()  # Проверка на NaN
        assert not torch.isinf(q_values).any()  # Проверка на бесконечность

    def test_forward_pass_single_input(self, dummy_model, config):
        """Тестирует прямой проход одного набора данных через сеть в режиме оценки."""
        view_indices = torch.randint(
            0,
            config.NUM_OBJECT_CODES,
            (config.VIEW_SIZE, config.VIEW_SIZE),  # Без батч-размерности
            dtype=torch.long,
        ).to(config.DEVICE)
        agent_state = torch.randn(config.AGENT_STATE_DIM, dtype=torch.float32).to(
            config.DEVICE
        )

        dummy_model.eval()  # Устанавливаем режим оценки
        with torch.no_grad():  # Отключаем вычисление градиентов
            q_values = dummy_model(view_indices, agent_state)

        assert isinstance(q_values, torch.Tensor)
        assert q_values.shape == (
            1,
            config.NUM_ACTIONS,
        )  # Ожидаем батч-размерность 1 на выходе
        assert q_values.device.type == config.DEVICE


class TestEnvironment:
    """Тесты для класса окружения `Environment`."""

    def test_init(self, env, config, simple_map):
        """Тестирует инициализацию окружения, размещение агентов и объектов."""
        assert env.height == config.MAP_HEIGHT
        assert env.width == config.MAP_WIDTH
        assert len(env.agents) == 2  # Peaceful + Predator из simple_map
        assert len(env.peaceful_agents) == 1
        assert len(env.predator_agents) == 1
        assert len(env.food_locations) == 1
        assert (2, 2) in env.food_locations
        assert env.base_map[0, 0] == OBSTACLE_CODE
        assert env.base_map[1, 1] == WATER_CODE

        p_agent = list(env.peaceful_agents.values())[0]
        r_agent = list(env.predator_agents.values())[0]
        assert p_agent.pos == (3, 3)
        assert r_agent.pos == (4, 4)
        assert np.array_equal(env.shared_map, simple_map)

    def test_get_local_view(self, env, config):
        """Тестирует получение локального поля зрения для агента."""
        agent_pos = (3, 3)  # Позиция мирного агента из simple_map
        padded_map = (
            env.get_padded_map()
        )  # Карта с отступами для корректного вида на границах
        view = env.get_local_view(agent_pos, padded_map)

        assert view.shape == (config.VIEW_SIZE, config.VIEW_SIZE)
        assert view.dtype == np.int64
        center_idx = config.VIEW_SIZE // 2
        assert (
            view[center_idx, center_idx] == PEACEFUL_CODE
        )  # В центре должен быть сам агент
        assert (
            view[center_idx - 1, center_idx - 1] == FOOD_CODE
        )  # Еда в (2,2), агент в (3,3)

    def test_get_agent_state(self, env, config):
        """Тестирует получение полного состояния агента (локальный вид + вектор состояния)."""
        peaceful_id = list(env.peaceful_agents.keys())[0]
        state = env.get_agent_state(peaceful_id)
        assert state is not None
        view, state_vec = state

        assert isinstance(view, np.ndarray)
        assert isinstance(state_vec, np.ndarray)
        assert view.shape == (env.config.VIEW_SIZE, env.config.VIEW_SIZE)
        assert state_vec.shape == (env.config.AGENT_STATE_DIM,)
        # Проверяем нормализованный голод (из peaceful_agent fixture, hunger=80)
        expected_norm_hunger = (0.8 * config.MAX_HUNGER) / config.MAX_HUNGER
        assert state_vec[0] == pytest.approx(expected_norm_hunger)

    def test_step_basic_movement(self, env, config):
        """Тестирует базовое перемещение агентов в окружении и обновление карты."""
        peaceful_id = list(env.peaceful_agents.keys())[0]
        predator_id = list(env.predator_agents.keys())[0]
        initial_peaceful_pos = env.agents[peaceful_id].pos
        initial_predator_pos = env.agents[predator_id].pos

        actions = {peaceful_id: 1, predator_id: 0}  # Мирный идет вверх, хищник стоит
        next_states, rewards, dones = env.step(actions)

        assert peaceful_id in next_states
        assert predator_id in next_states
        assert env.agents[peaceful_id].pos == (
            initial_peaceful_pos[0] - 1,  # Движение вверх (уменьшение y)
            initial_peaceful_pos[1],
        )
        assert env.agents[predator_id].pos == initial_predator_pos  # Хищник не двигался

        assert isinstance(rewards[peaceful_id], float)
        assert isinstance(rewards[predator_id], float)
        assert rewards[predator_id] == pytest.approx(
            config.REWARD_STEP
        )  # Награда за шаг
        assert not dones[peaceful_id]
        assert not dones[predator_id]

        # Проверка обновления общей карты
        assert env.shared_map[initial_peaceful_pos] == SURFACE_CODE
        assert env.shared_map[env.agents[peaceful_id].pos] == PEACEFUL_CODE

    def test_step_eat_food(self, env, config):
        """Тестирует поедание пищи мирным агентом, изменение голода и удаление еды с карты."""
        peaceful_id = list(env.peaceful_agents.keys())[0]
        agent = env.agents[peaceful_id]
        original_pos = agent.pos  # (3,3)
        agent.hunger = 75.0
        # Перемещаем агента рядом с едой (еда в (2,2))
        agent.pos = (2, 1)
        env.shared_map[original_pos] = env._get_terrain_code_from_base(*original_pos)
        env.shared_map[agent.pos] = PEACEFUL_CODE

        initial_hunger = agent.hunger
        initial_food_count = len(env.food_locations)
        assert (2, 2) in env.food_locations

        actions = {peaceful_id: 4}  # Движение вправо, на клетку с едой (2,2)
        next_states, rewards, dones = env.step(actions)

        assert agent.pos == (2, 2)  # Агент переместился на клетку с едой
        assert (
            agent.hunger > initial_hunger
        )  # Голод должен уменьшиться (сытость увеличиться)
        # Расчет ожидаемого голода: начальный - расход_за_шаг + прирост_от_еды * множитель_ценности_еды
        expected_hunger = min(
            initial_hunger
            - config.HUNGER_DECAY_STEP
            + config.REWARD_EAT_FOOD * 0.8,  # 0.8 - food_value_multiplier
            config.MAX_HUNGER,
        )
        assert agent.hunger == pytest.approx(expected_hunger)
        assert (
            rewards[peaceful_id] > config.REWARD_STEP
        )  # Награда за еду должна быть положительной
        assert len(env.food_locations) == initial_food_count - 1  # Еда должна исчезнуть
        assert (2, 2) not in env.food_locations
        assert env.shared_map[2, 1] == SURFACE_CODE  # Старая позиция агента
        assert env.shared_map[2, 2] == PEACEFUL_CODE  # Новая позиция агента (съел еду)

    def test_step_predator_eats_peaceful(self, env, config):
        """Тестирует поедание мирного агента хищником, обновление состояний и наград."""
        peaceful_id = list(env.peaceful_agents.keys())[0]
        predator_id = list(env.predator_agents.keys())[0]
        predator = env.agents[predator_id]
        peaceful = env.agents[peaceful_id]

        # Устанавливаем позиции для взаимодействия
        pred_orig_pos = predator.pos
        peace_orig_pos = peaceful.pos
        env.shared_map[pred_orig_pos] = env._get_terrain_code_from_base(*pred_orig_pos)
        env.shared_map[peace_orig_pos] = env._get_terrain_code_from_base(
            *peace_orig_pos
        )
        predator.pos = (3, 2)  # Хищник рядом с мирным
        peaceful.pos = (3, 3)  # Мирный
        env.shared_map[predator.pos] = PREDATOR_CODE
        env.shared_map[peaceful.pos] = PEACEFUL_CODE

        initial_predator_hunger = predator.hunger
        initial_peaceful_count = len(env.peaceful_agents)

        actions = {
            predator_id: 4,
            peaceful_id: 0,
        }  # Хищник двигается на мирного, мирный стоит

        next_states, rewards, dones = env.step(actions)

        assert predator_id in dones, "Predator ID should be in dones dict"
        assert not dones[predator_id], "Predator should not be done"
        assert predator.alive, "Predator should be alive"
        assert predator.pos == (3, 3), "Predator should have moved onto prey's position"
        assert (
            predator.hunger > initial_predator_hunger
        ), "Predator hunger should increase"
        assert (
            rewards[predator_id] > config.REWARD_EAT_PREY / 2
        ), "Predator reward should be significantly positive"  # Учитываем decay

        assert peaceful_id in dones, f"Peaceful agent {peaceful_id} not in dones dict"
        assert (
            dones[peaceful_id] is True
        ), f"Peaceful agent {peaceful_id} should be done (eaten)"
        assert (
            peaceful_id not in env.agents
        ), "Eaten peaceful agent should be removed from env.agents"
        assert (
            peaceful_id not in env.peaceful_agents
        ), "Eaten peaceful agent should be removed from env.peaceful_agents"
        assert len(env.peaceful_agents) == initial_peaceful_count - 1
        assert (
            rewards[peaceful_id]
            <= config.REWARD_DEATH  # Награда за смерть (отрицательная)
        ), "Peaceful agent reward should reflect death penalty"
        assert (
            peaceful.alive is False  # Флаг 'alive' должен быть False
        ), "Peaceful agent object 'alive' flag should be False"

        assert (
            env.shared_map[3, 2] == SURFACE_CODE  # Старая позиция хищника
        ), "Old predator pos should be empty (surface)"
        assert (
            env.shared_map[3, 3] == PREDATOR_CODE  # Хищник занял позицию съеденного
        ), "Predator should occupy the interaction position"

    def test_step_starvation(self, env, config):
        """Тестирует смерть агента от голода."""
        peaceful_id = list(env.peaceful_agents.keys())[0]
        agent = env.agents[peaceful_id]
        agent.hunger = config.HUNGER_DECAY_STEP / 2  # Голод почти на нуле
        initial_pos = agent.pos

        actions = {peaceful_id: 0}  # Агент стоит на месте
        next_states, rewards, dones = env.step(actions)

        assert dones[peaceful_id] is True  # Агент должен умереть
        assert peaceful_id not in env.agents  # Агент должен быть удален
        assert rewards[peaceful_id] <= config.REWARD_DEATH  # Награда за смерть
        assert env.shared_map[initial_pos] == env._get_terrain_code_from_base(
            *initial_pos
        )  # Клетка, где был агент, должна стать пустой (поверхностью)

    def test_step_reproduction(self, env, config):
        """Тестирует размножение агентов при достижении порога сытости."""
        peaceful_id = list(env.peaceful_agents.keys())[0]
        predator_id = list(env.predator_agents.keys())[0]  # Нужен для словаря actions
        agent = env.agents[peaceful_id]

        # Перемещаем агента на свободное место и обеспечиваем свободную клетку для потомка
        original_pos = agent.pos
        agent.pos = (5, 5)
        env.shared_map[original_pos] = env._get_terrain_code_from_base(*original_pos)
        env.shared_map[agent.pos] = PEACEFUL_CODE
        birth_pos_candidate = (5, 6)  # Кандидат на место рождения
        env.shared_map[
            birth_pos_candidate
        ] = SURFACE_CODE  # Убеждаемся, что клетка свободна

        agent.hunger = (
            config.REPRODUCTION_THRESHOLD + 1.0
        )  # Голод выше порога размножения
        initial_hunger = agent.hunger
        initial_peaceful_count = len(env.peaceful_agents)
        initial_agent_count = len(env.agents)

        actions = {predator_id: 0, peaceful_id: 0}  # Агенты стоят на месте
        next_states, rewards, dones = env.step(actions)

        assert (
            len(env.peaceful_agents) == initial_peaceful_count + 1
        )  # Должен появиться новый мирный агент
        assert len(env.agents) == initial_agent_count + 1
        assert round(agent.hunger) == initial_hunger * 0.5  # Голод родителя уменьшается
        assert (
            rewards[peaceful_id] > 0
        )  # Положительная награда за размножение (или стандартный шаг)
        assert dones[peaceful_id] is False

        # Ищем нового агента
        new_agent_found = False
        for new_id, new_agent_obj in env.agents.items():
            if (
                new_id != peaceful_id  # Не сам родитель
                and new_agent_obj.type_code == PEACEFUL_CODE
                and new_agent_obj.id
                > agent.id  # У нового агента должен быть больший ID
            ):
                assert (
                    new_agent_obj.pos == birth_pos_candidate
                )  # Проверяем позицию нового агента
                assert (
                    env.shared_map[new_agent_obj.pos] == PEACEFUL_CODE
                )  # Клетка занята новым агентом
                assert new_agent_obj.hunger == pytest.approx(
                    agent.hunger * 0.6
                )  # Голод потомка
                new_agent_found = True
                break
        assert new_agent_found, "New agent was not found after reproduction"

    def test_reset(self, env, simple_map, config):
        """Тестирует сброс окружения к начальному состоянию (как в `simple_map`)."""
        peaceful_id = list(env.peaceful_agents.keys())[0]
        actions = {
            peaceful_id: 1
        }  # Производим какое-то действие, чтобы изменить состояние
        env.step(actions)
        # Убеждаемся, что состояние изменилось
        assert not np.array_equal(env.shared_map, simple_map) or env.agents[
            peaceful_id
        ].pos != (3, 3)

        env.reset()  # Сбрасываем окружение

        assert np.array_equal(
            env.shared_map, simple_map
        )  # Карта должна вернуться к исходной
        assert len(env.agents) == 2
        assert len(env.peaceful_agents) == 1
        assert len(env.predator_agents) == 1
        p_agent = list(env.peaceful_agents.values())[0]
        r_agent = list(env.predator_agents.values())[0]
        assert p_agent.pos == (3, 3)  # Позиции агентов должны сброситься
        assert r_agent.pos == (4, 4)
        assert p_agent.hunger == pytest.approx(
            config.MAX_HUNGER * 0.8
        )  # Голод сбрасывается
        assert r_agent.hunger == pytest.approx(config.MAX_HUNGER * 0.8)
        assert len(env.food_locations) == 1  # Еда на месте
        assert (2, 2) in env.food_locations


class TestSimulationWorker:
    """Тесты для `SimulationWorker`, который управляет логикой симуляции и обучением."""

    def test_init(self, simulation_worker, config):
        """Тестирует инициализацию воркера, его моделей, буферов и окружения."""
        assert simulation_worker.config == config
        assert isinstance(simulation_worker.environment, Environment)
        assert isinstance(simulation_worker.peaceful_model, EmbeddingDuelingAgentNN)
        assert isinstance(simulation_worker.predator_model, EmbeddingDuelingAgentNN)
        assert isinstance(simulation_worker.peaceful_buffer, PrioritizedReplayBuffer)
        assert isinstance(simulation_worker.predator_buffer, PrioritizedReplayBuffer)
        assert simulation_worker.device == torch.device(config.DEVICE)

    def test_get_epsilon(self, simulation_worker, config):
        """Тестирует расчет значения эпсилон (для эпсилон-жадной стратегии) в зависимости от шага."""
        simulation_worker.global_step = 0
        assert simulation_worker._get_epsilon() == pytest.approx(config.EPS_START)

        simulation_worker.global_step = (
            config.EPS_DECAY_STEPS // 2
        )  # На полпути затухания
        expected_eps = config.EPS_START + 0.5 * (config.EPS_END - config.EPS_START)
        assert simulation_worker._get_epsilon() == pytest.approx(expected_eps)

        simulation_worker.global_step = (
            config.EPS_DECAY_STEPS * 2
        )  # После полного затухания
        assert simulation_worker._get_epsilon() == pytest.approx(config.EPS_END)

    @patch("random.random")
    @patch("random.randrange")
    def test_select_action_epsilon(
        self, mock_randrange, mock_random, simulation_worker, dummy_model, config
    ):
        """Тестирует выбор случайного действия (исследовательская часть эпсилон-жадной стратегии)."""
        mock_random.return_value = 0.0  # Гарантирует срабатывание ветки случайного выбора (if random.random() < epsilon)
        mock_randrange.return_value = 3  # Возвращаемое случайное действие
        simulation_worker.current_epsilon = 0.1  # Небольшой эпсилон
        agent_id = 1
        view = np.zeros((config.VIEW_SIZE, config.VIEW_SIZE), dtype=np.int64)
        state_vec = np.zeros(config.AGENT_STATE_DIM, dtype=np.float32)
        state = (view, state_vec)

        action = simulation_worker._select_action(agent_id, dummy_model, state)
        mock_randrange.assert_called_once_with(
            config.NUM_ACTIONS
        )  # Проверяем, что было вызвано случайное действие
        assert action == 3

    @patch("random.random")
    def test_select_action_greedy(
        self, mock_random, simulation_worker, dummy_model, config
    ):
        """Тестирует выбор жадного действия (на основе предсказаний модели)."""
        mock_random.return_value = 1.0  # Гарантирует срабатывание ветки жадного выбора (if random.random() >= epsilon)
        simulation_worker.current_epsilon = 0.9  # Большой эпсилон, но random > epsilon
        agent_id = 1
        # Создаем тензоры на нужном устройстве
        view = torch.zeros((config.VIEW_SIZE, config.VIEW_SIZE), dtype=torch.long).to(
            config.DEVICE
        )
        state_vec = torch.zeros(config.AGENT_STATE_DIM, dtype=torch.float32).to(
            config.DEVICE
        )
        state_np = (
            view.cpu().numpy(),
            state_vec.cpu().numpy(),
        )  # _select_action ожидает numpy
        # Мокаем Q-значения, чтобы действие 1 было оптимальным
        mock_q_values = torch.tensor([[0.1, 0.5, 0.2, 0.3, 0.4]], device=config.DEVICE)

        with patch.object(
            dummy_model, "forward", return_value=mock_q_values
        ) as mock_forward:
            action = simulation_worker._select_action(agent_id, dummy_model, state_np)
            mock_forward.assert_called_once()
            args, kwargs = mock_forward.call_args
            # Проверяем типы и устройства входных данных для модели
            assert args[0].dtype == torch.long
            assert args[1].dtype == torch.float32
            assert args[0].device.type == config.DEVICE
            assert args[1].device.type == config.DEVICE
            assert action == 1  # Действие с максимальным Q-значением

    def test_preprocess_batch(self, simulation_worker, dummy_experience, config):
        """Тестирует предобработку батча опыта: конвертацию в тензоры и разделение на компоненты."""
        batch = [
            dummy_experience
        ] * config.BATCH_SIZE  # Создаем батч из одинаковых опытов
        processed = simulation_worker._preprocess_batch(batch)
        assert processed is not None
        (
            states,
            actions_t,
            rewards_t,
            next_states,
            dones_mask,
            non_final_mask,
        ) = processed

        state_views_t, state_vectors_t = states
        next_state_views_t, next_state_vectors_t = next_states

        # Проверка форм
        assert state_views_t.shape == (
            config.BATCH_SIZE,
            config.VIEW_SIZE,
            config.VIEW_SIZE,
        )
        assert state_vectors_t.shape == (config.BATCH_SIZE, config.AGENT_STATE_DIM)
        assert actions_t.shape == (config.BATCH_SIZE, 1)
        assert rewards_t.shape == (config.BATCH_SIZE, 1)
        assert next_state_views_t.shape == (
            config.BATCH_SIZE,
            config.VIEW_SIZE,
            config.VIEW_SIZE,
        )
        assert next_state_vectors_t.shape == (config.BATCH_SIZE, config.AGENT_STATE_DIM)
        assert dones_mask.shape == (config.BATCH_SIZE, 1)
        assert non_final_mask.shape == (config.BATCH_SIZE,)

        # Проверка типов данных
        assert state_views_t.dtype == torch.long
        assert state_vectors_t.dtype == torch.float32
        assert actions_t.dtype == torch.long
        assert rewards_t.dtype == torch.float32
        assert dones_mask.dtype == torch.bool
        assert non_final_mask.dtype == torch.bool

        # Проверка устройства
        assert state_views_t.device.type == config.DEVICE
        assert state_vectors_t.device.type == config.DEVICE
        assert actions_t.device.type == config.DEVICE
        assert rewards_t.device.type == config.DEVICE

    @patch("ai_life_simulator._backend.agents.PrioritizedReplayBuffer.sample")
    @patch(
        "ai_life_simulator._backend.agents.PrioritizedReplayBuffer.update_priorities"
    )
    def test_train_step(
        self, mock_update_prio, mock_sample, simulation_worker, dummy_experience, config
    ):
        """Тестирует один шаг обучения модели, включая получение батча, расчет потерь и обновление приоритетов."""
        batch_size = config.BATCH_SIZE
        buffer = simulation_worker.peaceful_buffer
        model = simulation_worker.peaceful_model
        target_model = simulation_worker.peaceful_target_model
        optimizer = simulation_worker.peaceful_optimizer

        state, action, reward, next_state, done = dummy_experience
        exp = Experience(state, action, reward + 0.5, next_state, done)

        # Заполняем буфер, если он недостаточно полон для обучения
        if len(buffer) < config.MIN_BUFFER_SIZE_FOR_TRAIN:
            for i in range(config.MIN_BUFFER_SIZE_FOR_TRAIN - len(buffer) + batch_size):
                buffer.push(
                    abs(i * 0.1) + 0.1,
                    exp.state,
                    exp.action,
                    exp.reward + i,  # Разные награды для разнообразия
                    exp.next_state,
                    exp.done,
                )
        assert len(buffer) >= config.MIN_BUFFER_SIZE_FOR_TRAIN

        batch = [exp] * batch_size  # Мокаем батч
        # Мокаем индексы и веса IS, возвращаемые из sample
        start_idx = buffer.tree.capacity - 1  # Начало листьев в SumTree
        indices = list(range(start_idx, start_idx + batch_size))
        is_weights = np.ones(batch_size, dtype=np.float32)
        mock_sample.return_value = (batch, indices, is_weights)

        with patch.object(
            optimizer, "step", return_value=None
        ) as mock_step, patch.object(
            optimizer, "zero_grad", return_value=None
        ) as mock_zero_grad:
            loss = simulation_worker._train_step(model, target_model, buffer, optimizer)

            assert loss is not None
            assert isinstance(loss, float)
            mock_sample.assert_called_once_with(
                batch_size,
                simulation_worker.global_step,  # Проверяем вызов sample с правильными аргументами
            )
            mock_zero_grad.assert_called_once()  # Оптимизатор должен обнулить градиенты
            mock_step.assert_called_once()  # Оптимизатор должен сделать шаг
            mock_update_prio.assert_called_once()  # Приоритеты должны быть обновлены
            call_args, _ = mock_update_prio.call_args
            assert isinstance(call_args[0], list)  # Индексы
            assert len(call_args[0]) == batch_size
            assert isinstance(call_args[1], np.ndarray)  # Ошибки (TD-errors)
            assert len(call_args[1]) == batch_size

    def test_update_target_networks(self, simulation_worker, config):
        """Тестирует обновление весов целевых сетей с заданной частотой."""
        with patch.object(
            simulation_worker.peaceful_target_model, "load_state_dict"
        ) as mock_load_p, patch.object(
            simulation_worker.predator_target_model, "load_state_dict"
        ) as mock_load_r:
            # Шаг перед обновлением
            simulation_worker.global_step = config.TARGET_UPDATE_FREQ - 1
            simulation_worker._update_target_networks()
            mock_load_p.assert_not_called()
            mock_load_r.assert_not_called()

            # Шаг, на котором должно произойти обновление
            simulation_worker.global_step = config.TARGET_UPDATE_FREQ
            simulation_worker._update_target_networks()
            mock_load_p.assert_called_once()
            mock_load_r.assert_called_once()

            # Следующее обновление
            simulation_worker.global_step = config.TARGET_UPDATE_FREQ * 2
            simulation_worker._update_target_networks()
            assert mock_load_p.call_count == 2
            assert mock_load_r.call_count == 2

    def test_save_load_models(self, simulation_worker, tmp_path, config, simple_map):
        """Тестирует сохранение и загрузку моделей воркером."""
        save_dir = tmp_path / "models"
        simulation_worker.config.SAVE_MODEL_DIR = str(save_dir)
        simulation_worker.config.PEACEFUL_MODEL_FILENAME = "p_test.pth"
        simulation_worker.config.PREDATOR_MODEL_FILENAME = "r_test.pth"

        peaceful_path = save_dir / simulation_worker.config.PEACEFUL_MODEL_FILENAME
        predator_path = save_dir / simulation_worker.config.PREDATOR_MODEL_FILENAME

        simulation_worker.save_models(str(save_dir))  # Сохраняем модели
        assert peaceful_path.exists()
        assert predator_path.exists()

        # Создаем нового воркера для загрузки моделей
        shared_map_ref = simple_map.copy()
        new_worker = SimulationWorker(
            shared_map_ref, simulation_worker.config, simple_map
        )
        loaded_p, loaded_r = new_worker.load_models(str(save_dir))
        assert loaded_p is True
        assert loaded_r is True

        # Сравниваем state_dict загруженных моделей с сохраненными
        loaded_p_state = new_worker.peaceful_model.state_dict()
        loaded_r_state = new_worker.predator_model.state_dict()
        ref_p_state = torch.load(peaceful_path, map_location=config.DEVICE)
        ref_r_state = torch.load(predator_path, map_location=config.DEVICE)

        assert loaded_p_state.keys() == ref_p_state.keys()
        assert all(torch.equal(loaded_p_state[k], ref_p_state[k]) for k in ref_p_state)
        assert loaded_r_state.keys() == ref_r_state.keys()
        assert all(torch.equal(loaded_r_state[k], ref_r_state[k]) for k in ref_r_state)

        # Тест загрузки из несуществующей директории
        shutil.rmtree(save_dir)  # Удаляем сохраненные модели
        final_worker = SimulationWorker(
            shared_map_ref, simulation_worker.config, simple_map
        )
        loaded_p, loaded_r = final_worker.load_models(str(save_dir))
        assert loaded_p is False  # Модели не должны были загрузиться
        assert loaded_r is False


class TestSimulationAPI:
    """Тесты для `SimulationAPI`, предоставляющего интерфейс для управления симуляцией."""

    @patch("ai_life_simulator._backend.agents.SimulationWorker", autospec=True)
    @patch("threading.Thread", autospec=True)
    def test_start(self, mock_thread_class, mock_worker_class, simulation_api):
        """Тестирует запуск симуляции через API: создание воркера, загрузку моделей и запуск потока."""
        mock_worker_instance = mock_worker_class.return_value
        mock_worker_instance.load_models.return_value = (
            True,
            True,
        )  # Успешная загрузка моделей
        mock_worker_instance.command_queue = queue.Queue()  # Мокаем очереди
        mock_worker_instance.response_queue = queue.Queue()
        mock_thread_instance = mock_thread_class.return_value

        assert not simulation_api.is_running()  # Изначально API не запущено
        simulation_api.start()

        # Проверки вызовов
        mock_worker_class.assert_called_once_with(
            simulation_api.shared_map_for_viz,
            simulation_api.config,
            simulation_api.initial_user_map,
        )
        mock_worker_instance.load_models.assert_called_once_with(
            simulation_api.config.SAVE_MODEL_DIR
        )
        mock_thread_class.assert_called_once_with(
            target=mock_worker_instance.run, daemon=True, name="SimulationWorkerThread"
        )
        mock_thread_instance.start.assert_called_once()  # Поток должен быть запущен

        assert simulation_api.worker is mock_worker_instance
        assert simulation_api.worker_thread is mock_thread_instance
        mock_thread_instance.is_alive.return_value = True  # Мокаем, что поток жив
        assert simulation_api.is_running()  # API должно быть запущено

    @patch("ai_life_simulator._backend.agents.SimulationWorker", autospec=True)
    @patch("threading.Thread", autospec=True)
    def test_send_command(self, mock_thread_class, mock_worker_class, simulation_api):
        """Тестирует внутренний метод отправки команд воркеру, включая ожидание ответа и таймауты."""
        mock_worker_instance = mock_worker_class.return_value
        mock_worker_instance.command_queue = queue.Queue()
        mock_worker_instance.response_queue = queue.Queue()
        mock_thread_instance = mock_thread_class.return_value
        mock_thread_instance.is_alive.return_value = True  # Поток воркера активен

        simulation_api.worker = mock_worker_instance
        simulation_api.worker_thread = mock_thread_instance

        # Тест с ожиданием ответа
        mock_worker_instance.response_queue.put("response_ok")
        response = simulation_api._send_command(
            "cmd_test", wait_for_response=True, timeout=0.1
        )
        assert (
            mock_worker_instance.command_queue.get_nowait() == "cmd_test"
        )  # Команда должна быть в очереди
        assert response == "response_ok"

        # Тест с таймаутом ожидания ответа
        response = simulation_api._send_command(
            "cmd_timeout", wait_for_response=True, timeout=0.01
        )
        assert mock_worker_instance.command_queue.get_nowait() == "cmd_timeout"
        assert response is None  # Ответ не получен из-за таймаута

        # Тест без ожидания ответа
        response = simulation_api._send_command("cmd_no_wait", wait_for_response=False)
        assert mock_worker_instance.command_queue.get_nowait() == "cmd_no_wait"
        assert response == "command_sent"

        # Тест, когда воркер неактивен
        simulation_api.worker_thread.is_alive.return_value = False
        response = simulation_api._send_command("cmd_stopped")
        assert response is None
        assert (
            mock_worker_instance.command_queue.empty()
        )  # Команда не должна быть отправлена

    @patch.object(SimulationAPI, "_send_command")
    def test_api_commands(self, mock_send, simulation_api):
        """Тестирует высокоуровневые команды API (пауза, возобновление, сохранение и т.д.)."""
        simulation_api.worker = MagicMock()  # Мокаем воркера и поток
        simulation_api.worker_thread = MagicMock()
        simulation_api.worker_thread.is_alive.return_value = True

        simulation_api.pause()
        mock_send.assert_called_with("pause")

        simulation_api.resume()
        mock_send.assert_called_with("resume")

        simulation_api.save_models()  # Сохранение в директорию по умолчанию
        mock_send.assert_called_with(("save", simulation_api.config.SAVE_MODEL_DIR))

        custom_path = "/my/custom/path"
        simulation_api.save_models(path=custom_path)  # Сохранение по указанному пути
        mock_send.assert_called_with(("save", custom_path))

        simulation_api.get_epoch()
        mock_send.assert_called_with("get_epoch")

        simulation_api.get_step()
        mock_send.assert_called_with("get_step")


    @patch("threading.Thread", autospec=True)
    @patch.object(SimulationAPI, "_send_command")
    def test_stop(self, mock_send_command, mock_thread_class, simulation_api):
        """Тестирует остановку симуляции через API, включая отправку команды и ожидание завершения потока."""
        mock_thread_instance = mock_thread_class.return_value
        mock_thread_instance.is_alive.return_value = True
        simulation_api.worker = MagicMock()
        simulation_api.worker_thread = mock_thread_instance
        mock_send_command.return_value = "stopping_ack"
        test_timeout = 0.5
        response = simulation_api.stop(timeout=test_timeout)
        expected_send_timeout = max(1.0, test_timeout - 1.0)
        mock_send_command.assert_called_once_with(
            "stop", wait_for_response=True, timeout=pytest.approx(expected_send_timeout)
        )
        mock_thread_instance.join.assert_called_once_with(timeout=test_timeout)
        assert simulation_api.worker_thread is None
        assert simulation_api.worker is None
        assert response == "stopping_ack"

    def test_get_map_snapshot(self, simulation_api, simple_map):
        """Тестирует получение снимка (копии) текущего состояния карты."""
        np.copyto(
            simulation_api.shared_map_for_viz, simple_map
        )  # Инициализируем общую карту
        snapshot = simulation_api.get_map_snapshot()

        assert isinstance(snapshot, np.ndarray)
        assert np.array_equal(
            snapshot, simple_map
        )  # Снимок должен соответствовать исходной карте
        # Изменение снимка не должно влиять на общую карту API
        snapshot[0, 1] = 99
        assert simulation_api.shared_map_for_viz[0, 1] != 99

    @patch("threading.Thread", autospec=True)
    def test_is_running(self, mock_thread_class, simulation_api):
        """Тестирует проверку статуса работы симуляции (запущена/не запущена)."""
        assert not simulation_api.is_running(), "Should not be running initially"

        mock_thread_instance = mock_thread_class.return_value
        simulation_api.worker_thread = mock_thread_instance  # Назначаем мок потока

        mock_thread_instance.is_alive.return_value = True
        assert (
            simulation_api.is_running()
        ), "Should be running after thread assignment and is_alive=True"

        mock_thread_instance.is_alive.return_value = False
        assert (
            not simulation_api.is_running()
        ), "Should not be running when is_alive=False"

        simulation_api.worker_thread = None  # Убираем поток
        assert (
            not simulation_api.is_running()
        ), "Should not be running when thread is None"
