from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional, Tuple, Union, List

import ecl_data_io
import numpy as np
import numpy.typing as npt
import xtgeo


@dataclass
class FakeGrid:
    ncol: int
    nrow: int
    nlay: int
    actnum: npt.NDArray

    def __post_init__(self):
        self.dimensions = (self.nrow, self.ncol, self.nlay)

    def get_actnum(self):
        @dataclass
        class Mask:
            values: Iterable[bool]

        return Mask(values=self.actnum)


def readMask(
    grid_file: Path, shape: Optional[Tuple[int, int, int]] = None
) -> npt.NDArray[bool]:
    actnum, shape = _readMask(grid_file, shape)
    if actnum is None:
        actnum = np.zeros(shape, dtype=bool)

    return actnum, shape


def _readMask(
    grid_file: Path, shape: Optional[Tuple[int, int, int]] = None
) -> Tuple[Optional[npt.NDArray], Tuple[int, int, int]]:
    actnum = None
    shape_from_grid = None
    actnum_coords: List[Tuple[int, int, int]] = []
    with open(grid_file, "rb") as f:
        for entry in ecl_data_io.lazy_read(f):
            if actnum is not None and shape_from_grid is not None:
                break

            keyword = str(entry.read_keyword()).strip()
            if keyword == "ACTNUM":
                actnum = entry.read_array()
            elif keyword == "COORDS":
                coord_array = entry.read_array()
                if coord_array[4]:
                    actnum_coords.append(
                        (coord_array[0], coord_array[1], coord_array[2])
                    )
            elif shape_from_grid is None:
                if keyword == "GRIDHEAD":
                    arr = entry.read_array()
                    shape_from_grid = (arr[1], arr[2], arr[3])
                elif keyword == "DIMENS":
                    arr = entry.read_array()
                    shape_from_grid = (arr[0], arr[1], arr[2])

    if not shape:
        shape = shape_from_grid

    if actnum is None and actnum_coords:
        actnum = np.full(shape_from_grid, True, dtype=bool)
        for coord in actnum_coords:
            actnum[coord[0] - 1, coord[1] - 1, coord[2] - 1] = False
    elif actnum is not None:
        actnum = np.ascontiguousarray(np.logical_not(actnum.reshape(shape, order="F")))

    return actnum, shape


def maskData(
    data: npt.NDArray, grid_file: Path, shape: Optional[Tuple[int, int, int]]
) -> np.ma.MaskedArray:
    actnum, shape = _readMask(grid_file, shape)
    if actnum is None:
        actnum = np.zeros(shape, dtype=bool)
    return np.ma.MaskedArray(data=data, mask=actnum, fill_value=np.nan)


def readMaskedField(
    data_file: Union[Path, str],
    field_name: str,
    grid_file: Path,
    shape: Optional[Tuple[int, int, int]] = None,
) -> npt.NDArray:
    mask, shape = readMask(grid_file, shape)

    grid = FakeGrid(
        nrow=shape[0], ncol=shape[1], nlay=shape[2], actnum=np.logical_not(mask)
    )
    data = xtgeo.gridproperty_from_file(pfile=data_file, name=field_name, grid=grid)

    return data.values3d.filled(fill_value=np.nan)
