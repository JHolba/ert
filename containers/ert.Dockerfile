FROM ubuntu:22.04
WORKDIR /opt/ert
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean \
    && apt-get update -y \
    && apt-get install python3 python3-dev python3.10-venv cmake build-essential git jq -y 

# Create a virtual environment and "source" it
RUN python3 -m venv /opt/ert_venv
ENV PATH="/opt/ert_venv/bin:$PATH"
RUN pip install --upgrade pip 

COPY pyproject.toml .
# This part of config must be deleted, 
# or we will fail to get the dependencies with piptools
RUN sed -i '/\[tool\.setuptools_scm/,/^$/d' pyproject.toml

# Install dependencies
RUN --mount=type=cache,target=/root/.cache/pip pip install pip-tools conan
RUN --mount=type=cache,target=/root/.cache/pip \ 
    --mount=type=cache,target=/root/.cache/piptools \
    python -m piptools compile --cache-dir=/root/.cache/piptools --extra=.,dev --no-strip-extras -o - pyproject.toml \
    | pip install -r /dev/stdin --no-warn-conflicts
# Remove the modified pyproject before mounting ert folder
RUN rm pyproject.toml

# Build ert
RUN --mount=type=cache,target=/root/.cache/pip \
    --mount=type=bind,target=.,readwrite \
    rm -rf _skbuild \
    && pip install .

COPY test-data .

ENTRYPOINT [ "/bin/bash", "-c"]
CMD ["python -m ert test_run snake_oil/snake_oil.ert"]
