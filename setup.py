from setuptools import setup, Extension

setup(name='gym_superhexagon',
      packages=['gym_superhexagon', 'gym_superhexagon_native'],
      package_dir={"gym_superhexagon": "gym_superhexagon", "gym_superhexagon_native": "native"},
      package_data={'gym_superhexagon_native': ['libSDL2-2.0.so.0', 'superhexagon.x86_64.patch', 'shim.so', 'SuperHexagon_config.zip']},
      version='0.0.1',
      install_requires=['gym', 'scikit-image'],
      )
