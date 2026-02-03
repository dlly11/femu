# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import os
import sys

# Add the Python package to the path for autodoc
sys.path.insert(0, os.path.abspath("../python"))

# -- Project information -----------------------------------------------------
project = "FEMU"
copyright = "2024, FEMU Contributors"
author = "FEMU Contributors"
release = "0.1.0"

# -- General configuration ---------------------------------------------------
extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.napoleon",  # Google/NumPy docstring support
    "sphinx.ext.viewcode",  # Add links to source code
    "sphinx.ext.intersphinx",  # Link to other projects' docs
    "breathe",  # Doxygen integration
    "myst_parser",  # Markdown support
    "sphinx_autodoc_typehints",  # Better type hint rendering
]

# Templates path
templates_path = ["_templates"]

# Patterns to exclude
exclude_patterns = [
    "_build",
    "Thumbs.db",
    ".DS_Store",
]

# Master document
master_doc = "index"

# -- Options for HTML output -------------------------------------------------
html_theme = "furo"
html_static_path = ["_static"]
html_title = "FEMU Documentation"

# -- Breathe configuration (Doxygen integration) -----------------------------
breathe_projects = {"femu": "../build/docs/xml"}
breathe_default_project = "femu"

# -- MyST configuration (Markdown support) -----------------------------------
myst_enable_extensions = [
    "colon_fence",
    "deflist",
    "tasklist",
]

# Source file types
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

# -- Autodoc configuration ---------------------------------------------------
autodoc_default_options = {
    "members": True,
    "undoc-members": True,
    "show-inheritance": True,
}

# -- Intersphinx configuration -----------------------------------------------
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
}

# -- Napoleon configuration (docstring styles) -------------------------------
napoleon_google_docstring = True
napoleon_numpy_docstring = True
napoleon_include_init_with_doc = True

# -- Suppress warnings --------------------------------------------------------
# Suppress duplicate declaration warnings from Breathe (unavoidable when
# structs are defined in headers that include each other)
suppress_warnings = ["duplicate_declaration.cpp"]
