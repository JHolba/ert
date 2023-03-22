import os
from contextlib import contextmanager
from datetime import datetime
from io import BytesIO, TextIOWrapper
from pathlib import Path
from typing import Dict, Optional, Union
from uuid import UUID

from ecl.summary import EclSum

from ert.storage import LocalEnsembleAccessor, LocalStorageAccessor
from ert.storage.local_ensemble import _Index


class MemoryEnsembleAccessor(LocalEnsembleAccessor):
    @classmethod
    def create(
        cls,
        storage: LocalStorageAccessor,
        path: Path,
        uuid: UUID,
        *,
        ensemble_size: int,
        experiment_id: UUID,
        iteration: int = 0,
        name: str,
        prior_ensemble_id: Optional[UUID],
        refcase: Optional[EclSum],
    ) -> LocalEnsembleAccessor:
        abs_path = str(Path(path / "experiment/").absolute())
        mem = {abs_path: None}

        index = _Index(
            id=uuid,
            ensemble_size=ensemble_size,
            experiment_id=experiment_id,
            iteration=iteration,
            name=name,
            prior_ensemble_id=prior_ensemble_id,
            started_at=datetime.now(),
        )

        abs_path = str(Path(path / "index.json").absolute())
        mem[abs_path] = bytes(index.json(), "utf-8")

        return cls(storage, path, refcase=refcase, memory=mem)

    def __init__(
        self,
        storage: LocalStorageAccessor,
        path: Path,
        refcase: Optional[EclSum],
        memory: Dict[str, Optional[bytes]] = None,
    ):
        self._mem = memory if memory else {}
        super().__init__(storage, path, refcase)

    def _path_exists(self, path: Union[os.PathLike, str]) -> bool:
        return path in self._mem

    @contextmanager
    def _to_path(self, path: Union[os.PathLike, str]) -> Union[str, BytesIO]:
        path = str(Path(path).absolute())
        if path not in self._mem or path not in self._mem:
            self._mem[path] = bytes()
        file = BytesIO(self._mem[path])
        file.close = lambda: None
        yield file
        self._mem[path] = file.getvalue()
        BytesIO.close(file)

    def _mkdir(self, path, *args, **kwargs):
        path = str(Path(path).absolute())
        self._mem[path] = None

    @contextmanager
    def _open(self, path, *args, **kwargs) -> TextIOWrapper:
        with self._to_path(path) as file:
            yield TextIOWrapper(file, write_through=True, **kwargs)
