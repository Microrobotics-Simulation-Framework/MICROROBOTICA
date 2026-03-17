# Configuration file for the Sphinx documentation builder.
# MICROBOTICA documentation

project = 'MICROBOTICA'
copyright = '2026, MICROBOTICA Contributors'
author = 'MICROBOTICA Contributors'
release = '0.1.0'

extensions = [
    'myst_parser',
    'breathe',
    # 'sphinxcontrib.bibtex',  # Enable when bibliography.bib has entries
]

# MyST parser configuration
myst_enable_extensions = [
    'colon_fence',
    'deflist',
]
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

# Breathe configuration (Doxygen XML integration)
breathe_projects = {
    'MICROBOTICA': '_doxygen/xml',
}
breathe_default_project = 'MICROBOTICA'

# sphinxcontrib-bibtex configuration
# bibtex_bibfiles = ['bibliography.bib']

# --- Future multiproject mounts ---
# When MADDENING and MIME documentation is integrated, add their
# Sphinx projects here using sphinx-multiproject or intersphinx:
#
# intersphinx_mapping = {
#     'maddening': ('https://maddening.readthedocs.io/en/latest', None),
#     'mime': ('https://mime.readthedocs.io/en/latest', None),
# }

templates_path = ['_templates']
exclude_patterns = ['_build', '_doxygen', 'Thumbs.db', '.DS_Store']

html_theme = 'alabaster'
html_static_path = ['_static']
