### Build steps

- read in pyproject.toml tool.dspsim settings.
- run --json-only on all sources with no overrides to get default parameters.
- run --json-only again with overrides to get the final model params.
- generate bindings for all models
- generate include header for project.
- run verilator on all models
 