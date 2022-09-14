from setuptools import setup, find_packages, Extension
import sys
if sys.platform == 'win32':
    # hack to compile with clang
    # pr to add clang cl was never merged to distutils
    # https://github.com/pypa/distutils/pull/7
    import distutils._msvccompiler
    class ClangCl(distutils._msvccompiler.MSVCCompiler):
        def initialize(self):
            super().initialize()
            self.cc = 'clang-cl.exe'
            self.compile_options.append('-Wno-unused-variable')
            self.compile_options.append('-Wno-visibility')
            self.compile_options.append('-D_CRT_SECURE_NO_WARNINGS=1')
    distutils._msvccompiler.MSVCCompiler = ClangCl

extension = Extension(
    'packrect.packrect',
    sources = ['packrect.c'],
    include_dirs=['.'],
)
LONG_DESCRIPTION='''
This is a python binding to the public domain stb_rect_pack.

See https://github.com/nothings/stb/blob/master/stb_rect_pack.h

'''

setup(
    name='packrect',
    version='1.0.2',
    license='Public Domain',
    description='Pack Rectangles into a Rectangle',
    long_description=LONG_DESCRIPTION,
    long_description_content_type='text/markdown',
    # url='',
    author='David Priver',
    author_email='david@davidpriver.com',
    classifiers=[
        # 3 - Alpha
        # 4 - Beta
        # 5 - Production/Stable
        'Development Status :: 4 - Beta',
        'License :: Public Domain',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3 :: Only',
    ],
    packages=['packrect'],  # Required
    ext_modules = [extension],
    python_requires='>=3.6, <4',
    package_data={
        'packrect': ['py.typed', 'packrect.pyi'],
    },
)
