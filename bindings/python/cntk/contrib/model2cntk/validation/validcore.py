# ==============================================================================
# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root
# for full license information.
# ==============================================================================

import sys
import os
import numpy
import validation

from abc import ABCMeta, abstractmethod


class Validator(object):
    def __init__(self, global_conf, functions):
        self._source_solver = global_conf.source_solver
        self._valid_solver = global_conf.valid_solver
        self._functions = functions

    def val_network(self):
        if self._valid_solver.save_path is None:
            sys.stdout.write('ignore valid network...\n')
            return False
        val_inputs = [value for key, value in self._functions.items() if key in self._valid_solver.val_inputs]
        val_nodes = [value for key, value in self._functions.items() if key in self._valid_solver.val_nodes.keys()]
        if not os.path.exists(self._valid_solver.save_path):
            os.mkdir(self._valid_solver.save_path)

        def parser_save_path(dir_path, node_name):
            file_name = '.'.join((node_name, 'npy'))
            file_name = file_name.replace('\\', '.')
            file_name = file_name.replace('/', '.')
            return os.path.join(dir_path, file_name)

        valid_augments = {}
        for val_input in val_inputs:
            source_val_input = self._valid_solver.val_inputs[val_input.name][0]
            if len(source_val_input) == 2:
                [lower, upper] = source_val_input
                input_array = (upper - lower) * numpy.random.random_sample((1, ) + val_input.shape)
            else:
                input_array = numpy.array(source_val_input).reshape((1, ) + val_input.shape)
            valid_augments[val_input.name] = input_array
            save_path = parser_save_path(self._valid_solver.save_path, val_input.name)
            numpy.save(save_path, input_array)
        for val_node in val_nodes:
            used_augments = {augment: valid_augments[augment.name] for augment in val_node.arguments }
            val_results = val_node.forward(used_augments)
            output_array = list(val_results[1].values())[0]
            save_path = parser_save_path(self._valid_solver.save_path, self._valid_solver.val_nodes[val_node.name])
            numpy.save(save_path, output_array)

        return True

    def activate(self):
        validation.VALID_CORES[self._source_solver.source].execute(
            self._source_solver, self._valid_solver.save_path, self._valid_solver.val_inputs)


class ValidCore(object):
    __metaclass__ = ABCMeta

    @staticmethod
    @abstractmethod
    def execute(source_solver, valid_dir):
        pass

