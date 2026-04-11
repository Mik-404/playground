import torch
# Предполагается, что класс EmbeddingDuelingAgentNN и Config определены в agents.py
from ai_life_simulator._backend.agents import EmbeddingDuelingAgentNN, Config # Раскомментируйте, если используете как модуль

# --- Инициализация модели и конфигурации (примерные значения) ---
config = Config() # Используйте вашу реальную конфигурацию
model = EmbeddingDuelingAgentNN(
    view_size=config.VIEW_SIZE,
    state_dim=config.AGENT_STATE_DIM,
    num_actions=config.NUM_ACTIONS,
    num_object_codes=config.NUM_OBJECT_CODES,
    embedding_dim=config.EMBEDDING_DIM
)
model.eval() # Перевод модели в режим оценки

# Создание фиктивных входных данных
# Вам нужно будет определить view_size и state_dim из вашего конфига
# Например, если view_size = 7 и state_dim = 1 (только голод)
batch_size = 1 # Обычно для экспорта используется batch_size = 1
dummy_local_view_indices = torch.zeros(batch_size, config.VIEW_SIZE, config.VIEW_SIZE, dtype=torch.long)
dummy_agent_state = torch.randn(batch_size, config.AGENT_STATE_DIM, dtype=torch.float)

# Имена входов и выходов (опционально, но рекомендуется для понятности)
input_names = ["local_view_indices", "agent_state"]
output_names = ["q_values"]

# Экспорт модели
torch.onnx.export(
    model,
    (dummy_local_view_indices, dummy_agent_state), # Входные данные в виде кортежа
    "embedding_dueling_agent_nn.onnx",
    input_names=input_names,
    output_names=output_names,
    opset_version=11 # Выберите подходящую версию opset
)

print("Модель успешно экспортирована в embedding_dueling_agent_nn.onnx")