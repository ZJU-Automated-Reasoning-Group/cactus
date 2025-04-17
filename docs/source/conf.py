import os
import sys
import sphinx_rtd_theme

# Path setup
sys.path.insert(0, os.path.abspath('../../'))

# Project information
project = 'Cactus'
copyright = '2024'
author = 'Cactus Team'
version = '1.0'
release = '1.0.0'

# General configuration
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',
    'sphinx.ext.todo',
    'sphinx.ext.coverage',
    'sphinx.ext.mathjax',
    'sphinx.ext.viewcode',
    'sphinx.ext.napoleon',
    'sphinx_rtd_theme',
]

templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'
language = 'en'
exclude_patterns = []
pygments_style = 'sphinx'
todo_include_todos = True

# HTML output
html_theme = 'sphinx_rtd_theme'
html_theme_path = [sphinx_rtd_theme.get_html_theme_path()]
html_static_path = ['_static']
html_logo = None  # Add a logo file if available
html_favicon = None
html_show_sourcelink = True
html_show_sphinx = True
html_show_copyright = True

# Output options
htmlhelp_basename = 'Cactusdoc'

# LaTeX output
latex_elements = {}
latex_documents = [
    (master_doc, 'Cactus.tex', 'Cactus Documentation',
     'Cactus Team', 'manual'),
]

# Man page output
man_pages = [
    (master_doc, 'cactus', 'Cactus Documentation',
     [author], 1)
]

# Texinfo output
texinfo_documents = [
    (master_doc, 'Cactus', 'Cactus Documentation',
     author, 'Cactus', 'Static analysis framework for LLVM bitcode.',
     'Programming'),
]

# Intersphinx mapping
intersphinx_mapping = {'https://docs.python.org/3/': None} 