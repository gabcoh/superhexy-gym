from collections import namedtuple
from tempfile import mkstemp, mkdtemp
import pkg_resources
import os
import socket
import subprocess
import struct
import shutil
from multiprocessing import shared_memory
from contextlib import ExitStack
from zipfile import ZipFile

import gym
from skimage.transform import resize

import numpy as np

class SuperHexagonEnv(gym.Env):
    metadata = {'render.modes': ['rgb_array']}

    action_space = gym.spaces.Discrete(3)

    def __init__(self, superhex_dir=None, videodriver="offscreen", start_process=True, superhex_log=False, scale_factor=.16666666):
        if superhex_dir is None and "SUPERHEXAGON_DIR" in os.environ:
            superhex_dir = os.environ["SUPERHEXAGON_DIR"]
        self.superhex_dir = superhex_dir
        self.basedir = None
        self.configdir = None
        self.sock = None
        self.sock_file = None
        self.sock_fd = None
        self.server_sock = None
        self.proc = None
        self.width = None
        self.scale_factor = scale_factor
        self.raw_width = None
        self.height = None
        self.raw_height = None
        self.depth = None
        self.actions = None
        self.shm = None
        self.stack = None
        self.last_frame = None
        self.observation_space = None
        self.videodriver = videodriver
        self.start_process = start_process
        self.popen_kwargs = {}
        if not superhex_log:
                self.popen_kwargs['stdout'] = subprocess.DEVNULL
                self.popen_kwargs['stderr'] = subprocess.DEVNULL

        self._start()

    def _start(self):
        try:
            self.stack = ExitStack()
            self.stack.__enter__()

            self._create_configdir()
            self.stack.callback(lambda: self._close_configdir())

            self._create_basedir()
            self.stack.callback(lambda: self._close_basedir())

            self._create_server_sock_file()
            self.stack.callback(lambda: self._close_server_sock_file())

            self._create_server_sock()
            self.stack.callback(lambda: self._close_server_sock())

            self._create_proc()
            self.stack.callback(lambda: self._close_proc())
           

            self._create_shm()
            self.stack.callback(lambda: self._close_shm())
        except Exception as e:
            self.stack.close()
            raise e
        else:
            return self

    def _create_configdir(self):
        self.configdir = mkdtemp()
        with ZipFile(pkg_resources.resource_filename("gym_superhexagon_native", "SuperHexagon_config.zip")) as z:
            z.extractall(path=self.configdir)
    def _close_configdir(self):
        shutil.rmtree(self.configdir)

    def _create_basedir(self):
        self.basedir = mkdtemp()
        shutil.copytree(self.superhex_dir, self.basedir, dirs_exist_ok=True)
        shutil.copy(
            pkg_resources.resource_filename("gym_superhexagon_native", "libSDL2-2.0.so.0"),
            os.path.join(self.basedir, "x86_64/libSDL2-2.0.so.0")
        )

        if 0 != os.system("which bspatch"):
            raise gym.error.DependencyNotInstalled("bspatch not installed")

        os.system("bspatch {} {} {}".format(
            os.path.join(self.superhex_dir, "x86_64/superhexagon.x86_64"),
            os.path.join(self.basedir, "x86_64/superhexagon.x86_64"),
            pkg_resources.resource_filename("gym_superhexagon_native", "superhexagon.x86_64.patch")
        ))
    def _close_basedir(self):
        shutil.rmtree(self.basedir)

    def _create_server_sock_file(self):
        self.sock_fd, self.sock_file = mkstemp()
        os.unlink(self.sock_file)
    def _close_server_sock_file(self):
        os.unlink(self.sock_file)
        os.close(self.sock_fd)
        return

    def _create_server_sock(self):
        self.server_sock = socket.socket(family=socket.AF_UNIX, type=socket.SOCK_SEQPACKET)
        self.server_sock.bind(self.sock_file)
        self.server_sock.listen()
    def _close_server_sock(self):
        self.server_sock.shutdown(socket.SHUT_RDWR)
        self.server_sock.close()

    def _create_shm(self):
        init_form = "@iiii"
        w, h, d, a = struct.unpack(init_form, self.sock.recv(struct.calcsize(init_form)))
        self.raw_width = w
        self.raw_height = h
        self.width = round(w * self.scale_factor)
        self.height = round(h * self.scale_factor)
        self.depth = d
        self.observation_space = gym.spaces.Box(low=0, high=255, shape=(self.height, self.width, d), dtype=np.uint8)
        self.actions = a
        self.shm = shared_memory.SharedMemory(create=True, size=w*h*d)
        self.sock.send(struct.pack("@i64s",
                                   self.shm.size,
                                   self.shm.name.encode('ascii')
                                   ))
    def _close_shm(self):
        self.shm.close()
        self.shm.unlink()

    def _create_proc(self):
        if self.start_process:
            self.proc = subprocess.Popen(
                [os.path.join(self.basedir,
                            "x86_64/superhexagon.x86_64")
                ],
                cwd=self.basedir,
                env={
                    **os.environ,
                    "XDG_DATA_HOME": self.configdir,
                    "SDL_VIDEODRIVER": self.videodriver,
                    "SHIM_SOCKET": self.sock_file,
                    "LD_LIBRARY_PATH": os.path.join(self.basedir, "x86_64"),
                    "LD_PRELOAD": pkg_resources.resource_filename("gym_superhexagon_native", "shim.so")
                },
                **self.popen_kwargs
                )
        else:
            print("SHIM_SOCKET: ", self.sock_file)
        self.sock, _ = self.server_sock.accept()
    def _close_proc(self):
        if self.start_process:
            self.proc.kill()

    def _observe(self):
        form = "@if"
        data = self.sock.recv(struct.calcsize(form))
        if not data:
            raise BrokenPipeError("Connection to game has broken")
        terminal, reward = struct.unpack(form, data)
        self.last_frame = np.flip(np.reshape(
            np.frombuffer(self.shm.buf, dtype=np.uint8),
            (self.raw_height, self.raw_width, self.depth)), axis=0
                                          )
        self.last_frame = resize(self.last_frame, (self.height, self.width), anti_aliasing=False, preserve_range=True).astype(np.uint8)
        return terminal != 0, reward, self.last_frame
    def _send_action(self, a):
        if a < 0 or a >= self.actions:
            raise ValueError("Action must not be less than 0 or greater than self.actions")
        self.sock.send(struct.pack("@i", a))

    def step(self, a):
        self._send_action(a)

        done, reward, obs = self._observe()
        return obs, reward, done, {}
    def reset(self):
        self._close_shm()
        self._close_proc()
        self._close_server_sock()
        self._close_server_sock_file()

        self._create_server_sock_file()
        self._create_server_sock()
        self._create_proc()
        self._create_shm()

        _, _, obs = self._observe()
        return obs
    def render(self, mode="rgb_array"):
        if mode == 'rgb_array':
            return self.last_frame # return RGB frame suitable for video
        else:
            raise NotImplementedError(f"Mode: {mode}")
    def close(self):
        self.stack.close()
