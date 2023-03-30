import logging
from pathlib import Path
from typing import TYPE_CHECKING, Any, Tuple

from ert._c_wrappers.enkf.enkf_main import field_transform
from ert._c_wrappers.enkf.enkf_state import _internalize_results
from ert._c_wrappers.enkf.enums import ErtImplType, RealizationStateEnum
from ert._c_wrappers.enkf.model_callbacks import LoadStatus
from ert.storage.read_mask import readMaskedField

if TYPE_CHECKING:
    from ert._c_wrappers.enkf import EnsembleConfig, RunArg

logger = logging.getLogger(__name__)


def forward_model_ok(
    run_arg: "RunArg",
    ens_conf: "EnsembleConfig",
) -> Tuple[LoadStatus, str]:
    try:  # pylint: disable=R1702
        result = (LoadStatus.LOAD_SUCCESSFUL, "")
        if ens_conf.have_forward_init():
            forward_init_config_nodes = ens_conf.check_forward_init_nodes()
            error_msg = ""
            for config_node in forward_init_config_nodes:
                run_path = Path(run_arg.runpath)
                file_name = ens_conf.get_init_file_fmt(config_node.getKey())
                if "%d" in file_name:
                    file_name = file_name % run_arg.iens
                file_path = run_path / file_name

                if config_node.getImplementationType() == ErtImplType.SURFACE:
                    if file_path.exists():
                        run_arg.ensemble_storage.save_surface_file(
                            config_node.getKey(), run_arg.iens, str(file_path)
                        )
                    else:
                        error_msg += (
                            "Failed to initialize parameter "
                            f"'{config_node.getKey()}' in file {file_name}: "
                            "File not found\n"
                        )
                        result = (LoadStatus.LOAD_FAILURE, error_msg)

                    continue
                if config_node.getImplementationType() == ErtImplType.FIELD:
                    key = config_node.getKey()
                    if run_arg.ensemble_storage.field_has_data(key, run_arg.iens):
                        # Already initialised, ignore
                        continue

                    field_config = config_node.getFieldModelConfig()

                    data = readMaskedField(
                        file_path,
                        key,
                        ens_conf.grid_file,
                        (
                            field_config.get_nx(),
                            field_config.get_ny(),
                            field_config.get_nz(),
                        ),
                    )

                    trans = field_config.get_init_transform_name()
                    data_transformed = field_transform(data, trans)
                    run_arg.ensemble_storage.save_field(
                        key, run_arg.iens, data_transformed
                    )

        if result[0] == LoadStatus.LOAD_SUCCESSFUL:
            result = _internalize_results(ens_conf, run_arg)

    except Exception:
        logging.exception("Unhandled exception in callback for forward_model")
        result = (
            LoadStatus.LOAD_FAILURE,
            "Unhandled exception in callback for forward_model",
        )
    run_arg.ensemble_storage.state_map[run_arg.iens] = (
        RealizationStateEnum.STATE_HAS_DATA
        if result[0] == LoadStatus.LOAD_SUCCESSFUL
        else RealizationStateEnum.STATE_LOAD_FAILURE
    )

    return result


def forward_model_exit(run_arg: "RunArg", *_: Tuple[Any]) -> Tuple[Any, str]:
    run_arg.ensemble_storage.state_map[
        run_arg.iens
    ] = RealizationStateEnum.STATE_LOAD_FAILURE
    return (None, "")
