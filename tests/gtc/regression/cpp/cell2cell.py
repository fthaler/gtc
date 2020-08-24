# -*- coding: utf-8 -*-
#
# Cell to cell reduction.
# Note that the reduction refers to a LocationRef from outside!
#
# ```python
# for c1 in cells(mesh):
#     field1 = sum(f[c1] * f[c2] for c2 in cells(c1))
# ```

import os
import sys

from gtc.common import DataType
from gt_frontend.gtscript import Mesh, Field, Cell
from gt_frontend.frontend import GTScriptCompilationTask
from gtc.unstructured.usid_codegen import UsidGpuCodeGenerator, UsidNaiveCodeGenerator

dtype = DataType.FLOAT64

def sten(mesh : Mesh, field_in : Field[Cell, dtype], field_out : Field[Cell, dtype]):
    with computation(FORWARD), location(Cell) as c1:
        field_out = sum(field_in[c1]+field_in[c2] for c2 in cells(c1))

def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "unaive"

    if mode == "unaive":
        code_generator = UsidNaiveCodeGenerator
    else: # 'ugpu':
        code_generator = UsidGpuCodeGenerator

    generated_code = GTScriptCompilationTask(sten).compile(debug=True, code_generator=code_generator)

    print(generated_code)
    output_file = (
        os.path.dirname(os.path.realpath(__file__)) + "/generated_cell2cell_" + mode + ".hpp"
    )
    with open(output_file, "w+") as output:
        output.write(generated_code)


if __name__ == "__main__":
    main()
