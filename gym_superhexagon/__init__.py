from gym.envs.registration import register

from gym_superhexagon.superhexagon_env import SuperHexagonEnv

register(
    id='superhexagon-v0',
    entry_point='gym_superhexagon:SuperHexagonEnv',
)
