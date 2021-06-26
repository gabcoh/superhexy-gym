.PHONY: clean native install train tensorboard

native:
	make -C native

install: native
	python setup.py install

clean:
	make -C native clean

train:
	cd rl-baselines3-zoo && python train.py --algo dqn --env superhexagon-v0 --tensorboard-log runs --save-freq 25000  --vec-env subproc --n-eval-envs 4

tensorboard:
	python $(VIRTUAL_ENV)/lib/python3.9/site-packages/tensorboard/main.py --logdir ./runs/
