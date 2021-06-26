# Super Hexagon Gym Environment

An [OpenAI Gym](https://gym.openai.com/) environment for the classic (imho)
Terry Cavanagh game [SuperHexagon](https://superhexagon.com/)

## Usage

Note, that I have only tested on linux and for a variety of reasons, this
probably won't work on other platforms.

The interface is the OpenAI gym environment interface. The `rgb_array` rendering
method is the only one supported right now. The action space is `Dicrete(3)` and
the observation space is `Box(low=0, high=255, shape=(768*scale_factor,
480*scale_factor, 3), dtype=np.uint8)` (scale_factor described later). You can
look in the `tests` directory for some example code. The following are the
environment keyword arguments accepted:

| argument        | description                                                                                                                                                                                                                                                  |
|-----------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `superhex_dir`  | the path to the root of your installation of Super Hexagon. I'm pretty sure it's not legal to distribute the game, so you have to provide it yourself. Can also be set through the `SUPERHEXAGON_DIR` environment variable.                                  |
| `videodriver`   | determines the `SDL_VIDEODRIVER` environment variable passed to the Super Hexagon process. Defaults to `offscreen`. Set to the empty string to make the game visible, assuming your SDL build supports that.                                                 |
| `start_process` | Defaults to `True`. Whether to actually start a new Super Hexagon game process or try to connect to an existing shimmed process. Useful for debugging the shim.                                                                                              |
| `superhex_log`  | When false pipe the Super Hexagon subprocess output to `/dev/null`. Defaults to `False`                                                                                                                                                                      |
| `scale_factor`  | The Super Hexagon game will render at 768 by 480 by default. This determines the size of the observation made by the environment (useful to scale down when used as input to neural networks). Defaults to `.16666` making the observed resolution 128 by 80 |

## Instalation

## How

## TODO

- [x] Document other environment keyword args
- [ ] Document installation (don't forget to mention native dependencies like
      sdl and bspatch and ffmpeg for training)
- [ ] Document training with baselines zoo and tensorboard
- [ ] Document changes made in zoo fork
- [ ] Document how it works
