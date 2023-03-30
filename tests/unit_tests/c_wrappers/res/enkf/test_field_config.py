import pytest

from ert._c_wrappers.enkf.config import FieldConfig, FieldTypeEnum
from ert._c_wrappers.enkf.enums import EnkfFieldFileFormatEnum


def test_create():
    FieldConfig("SWAT", "path_to_file")


def test_field_type_enum():
    assert FieldTypeEnum.ECLIPSE_PARAMETER == FieldTypeEnum(2)
    gen = FieldTypeEnum.GENERAL
    assert str(gen) == "GENERAL"
    gen = FieldTypeEnum(3)
    assert str(gen) == "GENERAL"


def test_export_format():
    assert (
        FieldConfig.exportFormat("file.grdecl")
        == EnkfFieldFileFormatEnum.ECL_GRDECL_FILE
    )
    assert (
        FieldConfig.exportFormat("file.xyz.grdecl")
        == EnkfFieldFileFormatEnum.ECL_GRDECL_FILE
    )
    assert (
        FieldConfig.exportFormat("file.roFF") == EnkfFieldFileFormatEnum.RMS_ROFF_FILE
    )
    assert (
        FieldConfig.exportFormat("file.xyz.roFF")
        == EnkfFieldFileFormatEnum.RMS_ROFF_FILE
    )

    with pytest.raises(ValueError):
        FieldConfig.exportFormat("file.xyz")

    with pytest.raises(ValueError):
        FieldConfig.exportFormat("file.xyz")
