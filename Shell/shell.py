import shutil
from struct import pack
from typing import List
from xml.etree import ElementTree
from zipfile import ZipFile

from dex_file import JNIFuncBuilder
from env_settings import *
from util.AXMLUtil.axml import AndroidXML
from util.apk.apk import APK
from util.apktool.apktool import ApkTool


class ConfigFileProxy:
    log = logging.getLogger(__name__)
    HEAD_SIZE = 0x04 * 2 * 7 + 0x04 * 4 * 0

    def __init__(self):
        self.src_apk_application_name: str = ''
        self.tmp_out_path: str = ''

        self.src_classes_dex_name: str = 'classes.dex'
        self.fake_classes_dex_name: str = 'fake_classes0.dex'

        # assets/lib.zip
        self.src_lib_zip_name: str = 'lib'
        self.src_lib_so_name: str = 'so_name.bin'
        self.fake_lib_so_name: str = 'libfake.so'

        self.src_classes_dex_code_item_name: str = 'code_item.bin'
        self.jni_func_name: str = 'JNIFunc.cpp'
        self.jni_func_header_name: str = 'JNIFunc.h'

        self.config_file_name = 'config.bin'

    def write(self):
        """
        bin format:

        head:
            # 4*2
            uint32:     srcApkApplicationNameSize
            uint32:     srcApkApplicationNameOff

            # 4*2
            uint32:     fakeClassesDexNameSize
            uint32:     fakeClassesDexNameOff

            # 4*2
            uint32:     fakeLibSoNameSize
            uint32:     fakeLibSoNameOff

            # 4*2
            uint32:     srcLibNameSize
            uint32:     srcLibNameOff

            # 4*2
            uint32:     srcLibZipSize
            uint32:     srcLibZipOff

            # 4*2
            uint32:     srcClassesDexSize
            uint32:     srcClassesDexOff

            # 4*2
            uint32:     srcClassesCodeItemSize
            uint32:     srcClassesCodeItemOff


        data:
            char[]      srcApkApplicationName
            char[]      fakeClassesDexName
            char[]      fakeLibSoName
            char[]      srcLibName

            char[]      srcLibZip
            char[]      srcClassesDex
            char[]      srcClassesCodeItem

        """

        self.src_lib_zip_name += '.zip'
        src_apk_application_name, dst_path = self.src_apk_application_name, self.tmp_out_path

        dst_path = dst_path + '/assets'
        index_buf = bytearray()
        data_buf = bytearray()

        # write config
        self.__write_str(index_buf, data_buf, self.src_apk_application_name)
        self.__write_str(index_buf, data_buf, self.fake_classes_dex_name)
        self.__write_str(index_buf, data_buf, self.fake_lib_so_name)
        self.__write_file(index_buf, data_buf, dst_path + '/' + self.src_lib_so_name)
        self.__write_file(index_buf, data_buf, dst_path + '/' + self.src_lib_zip_name)
        self.__write_file(index_buf, data_buf, dst_path + '/' + self.src_classes_dex_name)
        self.__write_file(index_buf, data_buf, dst_path + '/' + self.src_classes_dex_code_item_name)

        # remove tmp files
        os.remove(dst_path + '/' + self.src_lib_so_name)
        os.remove(dst_path + '/' + self.src_lib_zip_name)
        os.remove(dst_path + '/' + self.src_classes_dex_name)
        os.remove(dst_path + '/' + self.src_classes_dex_code_item_name)

        with open(dst_path + '/' + self.config_file_name, 'wb') as writer:
            writer.write(index_buf)
            writer.write(data_buf)
            writer.flush()
            self.log.debug('write file: %s', dst_path + '/' + self.config_file_name)

        self.log.info('write finish...')

    @classmethod
    def __write_str(cls, index_buf: bytearray, data_buf: bytearray, str_data: str):
        index_buf.extend(pack('<2I', len(str_data) + 1, cls.HEAD_SIZE + len(data_buf)))
        cls.log.debug("size: %d, offset: %d", len(str_data), cls.HEAD_SIZE + len(data_buf))
        data_buf.extend(bytes(str_data, encoding='ascii'))
        data_buf.append(0)
        cls.log.debug(bytes(str_data, encoding='ascii'))

    @classmethod
    def __write_file(cls, index_buf: bytearray, data_buf: bytearray, file_path: str):
        index_buf.extend(pack('<2I', os.path.getsize(file_path), cls.HEAD_SIZE + len(data_buf)))
        cls.log.debug("size: %d, offset: %d", os.path.getsize(file_path), cls.HEAD_SIZE + len(data_buf))
        with open(file_path, 'rb') as reader:
            file_data = reader.read()
            data_buf.extend(file_data)


class Shell:
    REBUILD_SMALI_FILES_NAME = [
        'ShellApplication.smali'
    ]

    SHELL_DEPENDENCIES_JAR_PACKAGE_NAME = [
    ]

    log = logging.getLogger(__name__)

    def __init__(self, no_shell: bool = True):
        # src apk
        self.in_apk_name: str = ''
        self.in_apk_path: str = ''
        self.in_apk_pkg_name: str = ''
        self.in_apk_application_name: str = ''
        in_apk: str = os.listdir(APK_IN)[0]
        self.in_apk_name: str = in_apk[:-4]
        self.in_apk_path: str = APK_IN + '/' + in_apk

        # shell apk
        if no_shell:
            self.shell_apk_pkg_name: str = ''
            self.shell_apk_name: str = ''
            self.shell_apk_path: str = ''
            self.shell_apk_name: str = ''

            self.tmp_shell_path: str = ''
        else:
            self.shell_apk_pkg_name: str = ''
            self.shell_apk_name: str = os.listdir(APK_SHELL)[0]
            self.shell_apk_path: str = APK_SHELL + '/' + self.shell_apk_name
            self.shell_apk_name: str = self.shell_apk_name[:-4]

            self.tmp_shell_path: str = APK_TMP_SHELL + '/' + self.shell_apk_name

        # tmp files
        self.tmp_in_path: str = ''
        self.tmp_out_path: str = ''

        # config file proxy
        self.config_file = ConfigFileProxy()

    def update_shell_apk(self):
        # shell apk
        self.shell_apk_pkg_name: str = ''
        self.shell_apk_name: str = os.listdir(APK_SHELL)[0]
        self.shell_apk_path: str = APK_SHELL + '/' + self.shell_apk_name
        self.shell_apk_name: str = self.shell_apk_name[:-4]
        # tmp files
        self.tmp_shell_path: str = APK_TMP_SHELL + '/' + self.shell_apk_name

    def unzip_in_apk(self):
        in_path, in_name = self.in_apk_path, self.in_apk_name
        tmp_in_path = APK_TMP_IN + '/' + in_name
        tmp_out_path = APK_TMP_OUT + '/' + in_name
        with ZipFile(in_path) as apk:
            apk.extractall(tmp_in_path)
            self.log.info(
                'unzip file from {from_path} to {to_path}'.format(from_path=in_path, to_path=tmp_in_path))

        self.tmp_in_path = tmp_in_path
        self.tmp_out_path = tmp_out_path
        os.mkdir(tmp_out_path)
        os.mkdir(tmp_out_path + '/assets')
        self.log.debug('mkdir: %s', tmp_in_path)
        self.log.debug('mkdir: %s', tmp_out_path)
        self.log.debug('mkdir: %s', tmp_out_path + '/assets')
        self.log.info('unzip_in_apk finish...')

    def decompile_shell_apk(self):
        ApkTool.decode(self.shell_apk_path, self.tmp_shell_path, APKTOOL_PATH)
        self.log.info('decompile_shell_apk finish...')

    def modify_in_manifest(self):
        # get shell apk application's name and package's name
        manifest = ElementTree.parse(self.tmp_shell_path + '/AndroidManifest.xml').getroot()
        self.shell_apk_pkg_name = manifest.attrib['package']
        application = manifest.find('application')
        key = '{http://schemas.android.com/apk/res/android}name'
        shell_apk_application_name = application.attrib[key]
        if shell_apk_application_name[0] == '.':
            shell_apk_application_name = shell_apk_application_name[1:]
        else:
            shell_apk_application_name = shell_apk_application_name[len(self.shell_apk_pkg_name) + 1:]

        self.log.debug('shell_apk_application_name: %s', shell_apk_application_name)

        # get the in_apk's application name and modify it to shell application name
        in_path, out_path = self.tmp_in_path, self.tmp_out_path
        in_path += '/AndroidManifest.xml'
        out_path += '/AndroidManifest.xml'

        # read original in_apk's manifest file
        manifest_str = AndroidXML.axml2xml(in_path, AXML_PRINTER_PATH)
        manifest = ElementTree.fromstring(manifest_str[0])
        self.in_apk_pkg_name = manifest.attrib['package']
        application = manifest.find('application')
        if key in application.attrib:
            in_apk_application_name = application.attrib[key]
            if in_apk_application_name[0] == '.':
                self.in_apk_application_name = self.in_apk_pkg_name + in_apk_application_name
            else:
                self.in_apk_application_name = in_apk_application_name
        else:
            self.in_apk_application_name = 'android.app.Application'

        self.log.debug('src package name: ' + self.in_apk_pkg_name)
        self.log.debug('src application name: ' + self.in_apk_application_name)
        in_apk_changed_application_name = self.in_apk_pkg_name + '.' + shell_apk_application_name
        self.log.debug('src changed application name: ' + in_apk_changed_application_name)

        # modify in_apk's application name
        AndroidXML.modify_attr('application', 'package', 'name', in_apk_changed_application_name,
                               in_path, out_path, AXML_EDITOR_PATH)

        # print modified in_apk's manifest
        AndroidXML.print_xml(out_path, AXML_PRINTER_PATH)
        self.log.debug('create manifest: %s', out_path)
        self.log.info('modify_in_manifest finish...')

    def rebuild_shell_dex(self):
        # copy the shell_apk's smali files to modify them later
        tmp_copy_shell_apk_path = self.tmp_shell_path + 'copy'
        shutil.copytree(self.tmp_shell_path, tmp_copy_shell_apk_path)
        self.log.debug('create copy shell dir(decompiled)')

        # the dir path of shell_apk's smali files
        shell_smali_package_name = self.shell_apk_pkg_name.replace('.', '/')
        self.log.debug('shell_smali_package_name: %s', shell_smali_package_name)
        shell_smali_src_path = self.tmp_shell_path + '/smali/' + shell_smali_package_name
        shell_smali_dst_path = tmp_copy_shell_apk_path + '/smali'
        self.log.debug('shell_smali_src_path: %s', shell_smali_src_path)
        self.log.debug('shell_smali_dst_path: %s', shell_smali_dst_path)

        in_name, in_pkg_name, out_path = self.in_apk_name, self.in_apk_pkg_name, self.tmp_out_path
        # remove all original shell_apk's smali files
        shutil.rmtree(shell_smali_dst_path)
        os.mkdir(shell_smali_dst_path)

        # modify the package name in shell_apk's smali files and write them
        src_smali_package_name = in_pkg_name.replace('.', '/')
        self.log.debug('src_smali_package_name: ' + src_smali_package_name)
        for smali_file in self.REBUILD_SMALI_FILES_NAME:
            # read all smali files needed to rebuild
            with open(shell_smali_src_path + '/' + smali_file, 'r') as r:
                smali_data = r.read()

            with open(shell_smali_dst_path + '/' + smali_file, 'w') as w:
                new_smali_data = smali_data.replace(shell_smali_package_name, src_smali_package_name)
                w.write(new_smali_data)
                w.flush()
                self.log.debug('write file: ' + shell_smali_dst_path + '/' + smali_file)
                self.log.debug('smali data: \n' + new_smali_data)

        # copy third-lib's smali
        for lib in self.SHELL_DEPENDENCIES_JAR_PACKAGE_NAME:
            lib_smali_path = lib.replace('.', '/')
            lib_src_path = self.tmp_shell_path + '/smali/' + lib_smali_path
            lib_dst_path = shell_smali_dst_path + '/' + lib_smali_path
            shutil.copytree(lib_src_path, lib_dst_path)
            self.log.debug("copy third-lib's smali files from %s to %s", lib_src_path, lib_dst_path)

        # generate modified shell_apk's classes.dex
        tmp_rebuild_apk = self.tmp_shell_path + '.apk'
        ApkTool.build(tmp_copy_shell_apk_path, tmp_rebuild_apk, APKTOOL_PATH)
        with ZipFile(tmp_rebuild_apk) as apk:
            apk.extract('classes.dex', out_path)
            self.log.debug('create dex: %s/classes.dex', out_path)

        self.log.info('rebuild_shell_dex finish...')

    def encrypt_shell_so(self):
        in_path, out_path = self.tmp_shell_path + '/lib', self.tmp_out_path + '/lib'
        src_lib = self.tmp_in_path + '/lib'
        if not os.path.exists(src_lib):
            os.mkdir(src_lib)
        abis = os.listdir(src_lib)
        if abis:
            if not os.path.exists(out_path):
                os.mkdir(out_path)
                for abi in abis:
                    os.mkdir(out_path + '/' + abi)

            for abi in abis:
                in_so_path = in_path + '/' + abi
                out_so_path = out_path + '/' + abi
                for so_name in os.listdir(in_so_path):
                    shutil.copyfile(in_so_path + '/' + so_name, out_so_path + '/' + so_name)
        else:
            if os.path.exists(out_path):
                shutil.rmtree(out_path)
            shutil.copytree(in_path, out_path)

        self.log.debug('copy files from %s to %s', in_path, out_path)
        self.log.info('encrypt_shell_so finish...')

    def encrypt_in_so(self):
        in_path, out_path = self.tmp_in_path, self.tmp_out_path
        in_so_path = in_path + '/lib'
        out_so_path = out_path + '/assets/' + self.config_file.src_lib_zip_name
        in_so_names: List[str] = []
        if not os.path.exists(in_so_path):
            os.mkdir(in_so_path)
        else:
            abi = os.listdir(in_so_path)
            if abi:
                in_so_file_path = in_so_path + '/' + abi[0]
                in_so_names = os.listdir(in_so_file_path)

        lib_zip_path = shutil.make_archive(out_so_path, 'zip', in_so_path, logger=self.log)
        self.log.debug('create so zip file: %s', lib_zip_path)

        # write so name file
        so_name_file_path = out_path + '/assets/' + self.config_file.src_lib_so_name
        with open(so_name_file_path, 'wb') as writer:
            writer.write(pack('<I', len(in_so_names)))
            for name in in_so_names:
                writer.write(pack('<I', len(name) + 1))
                writer.write(bytes(name, encoding='ascii'))
                writer.write(b'\x00')
        self.log.debug('write file: %s to %s', self.config_file.src_lib_so_name, so_name_file_path)

        # cp the fake so file
        out_path = out_path + '/lib'
        if not os.path.exists(out_path):
            os.mkdir(out_path)

        for abi in os.listdir(in_so_path):
            fake_lib_so_file_path = \
                '{root_dir}/{abi}/' \
                '{fake_lib_so_file_name}'.format(root_dir=FAKE_LIB_SO_DIR, abi=abi,
                                                 fake_lib_so_file_name=FAKE_LIB_SO_FILE_NAME)
            out_so_path = out_path + '/' + abi
            if not os.path.exists(out_so_path):
                os.mkdir(out_so_path)

            out_so_path += '/' + self.config_file.fake_lib_so_name
            shutil.copyfile(fake_lib_so_file_path, out_so_path)
        self.log.debug('copy the fake so from %s to %s', FAKE_LIB_SO_DIR, out_path)
        self.log.info('encrypt_in_so finish...')

    def encrypt_src_dex(self):
        in_path, in_pkg_name, out_path = self.tmp_in_path, self.in_apk_pkg_name, self.tmp_out_path
        out_path += '/assets'

        manifest_str = AndroidXML.axml2xml(self.tmp_in_path + '/AndroidManifest.xml', AXML_PRINTER_PATH)
        manifest = ElementTree.fromstring(manifest_str[0])
        in_pkg_name = manifest.attrib['package']
        exclude_class_names = {
            in_pkg_name + '.BuildConfig',
            in_pkg_name + '.R$anim',
            in_pkg_name + '.R$attr',
            in_pkg_name + '.R$bool',
            in_pkg_name + '.R$color',
            in_pkg_name + '.R$dimen',
            in_pkg_name + '.R$drawable',
            in_pkg_name + '.R$id',
            in_pkg_name + '.R$integer',
            in_pkg_name + '.R$layout',
            in_pkg_name + '.R$mipmap',
            in_pkg_name + '.R$string',
            in_pkg_name + '.R$style',
            in_pkg_name + '.R$styleable',
            in_pkg_name + '.R',
        }

        # TODO Test
        include_pkg_names = [in_pkg_name]

        builder = JNIFuncBuilder(in_path + '/classes.dex')
        builder.filter_methods(include_pkg_names=include_pkg_names, exclude_class_names=exclude_class_names)
        builder.write(code_item_out_path=out_path + '/' + self.config_file.src_classes_dex_code_item_name,
                      dex_out_path=out_path + '/' + self.config_file.src_classes_dex_name,
                      jni_func_out_path=out_path + '/' + self.config_file.jni_func_name,
                      jni_func_header_out_path=out_path + '/' + self.config_file.jni_func_header_name)
        self.log.debug('create file: %s', out_path + '/' + self.config_file.src_classes_dex_code_item_name)
        self.log.debug('create file: %s', out_path + '/' + self.config_file.src_classes_dex_name)
        self.log.debug('create file: %s', out_path + '/' + self.config_file.jni_func_name)
        self.log.debug('create file: %s', out_path + '/' + self.config_file.jni_func_header_name)
        self.log.info('encrypt_src_dex, finish...')

    def copy_src_other_res(self):
        src_path, out_path = self.tmp_in_path, self.tmp_out_path
        files = set(os.listdir(src_path))
        if 'assets' in files:
            files.remove('assets')
            src = src_path + '/assets'
            dst = out_path + '/assets'
            for f in os.listdir(src):
                shutil.copy(src + '/' + f, dst)
            self.log.debug('copy files from %s to %s' % (src, dst))

        if 'lib' in files:
            files.remove('lib')

        if 'classes.dex' in files:
            files.remove('classes.dex')

        if 'META-INF' in files:
            files.remove('META-INF')

        if 'AndroidManifest.xml' in files:
            files.remove('AndroidManifest.xml')

        for f in files:
            f_path = src_path + '/' + f
            if os.path.isdir(f_path):
                shutil.copytree(f_path, out_path + '/' + f)
            else:
                shutil.copy(f_path, out_path)
            self.log.debug('copy other files from %s to %s' % (f_path, out_path + '/' + f))

        self.log.info('copy_src_other_res finish...')

    def rebuild_and_resign_apk(self):
        out_path, src_apk_name = self.tmp_out_path, self.in_apk_name
        self.config_file.src_apk_application_name = self.in_apk_application_name
        self.config_file.tmp_out_path = self.tmp_out_path
        self.config_file.write()

        # rebuild apk
        rebuild_zip_path = shutil.make_archive(out_path, 'zip', out_path, logger=self.log)
        self.log.debug('rebuild_zip_path: ' + rebuild_zip_path)
        rebuild_apk_path = APK_TMP_OUT + '/' + src_apk_name + '.apk'
        os.rename(rebuild_zip_path, rebuild_apk_path)
        self.log.debug('rebuild_apk_path: ' + rebuild_apk_path)

        apktool_decode_path = APK_TMP_OUT + '/' + src_apk_name + '_apktool'
        apktool_rebuild_path = APK_TMP_OUT + '/' + src_apk_name + '_apktool_unsigned.apk'
        ApkTool.decode(rebuild_apk_path, apktool_decode_path, APKTOOL_PATH)
        ApkTool.build(apktool_decode_path, apktool_rebuild_path, APKTOOL_PATH)

        # sign
        out_apk_path = APK_OUT + '/' + src_apk_name + '_signed.apk'
        if os.path.exists(out_apk_path):
            os.remove(out_apk_path)
        APK.sign(KEY_STORE_PATH, KEY_STORE_PWD, KEY_STORE_ALIAS, KEY_STORE_ALIAS_PWD,
                 apktool_rebuild_path, out_apk_path)
        self.log.debug('resign apk success: %s', out_apk_path)
        self.log.info('rebuild_and_resign_apk finish...')

    def remove_all_tmp(self):
        for tmp_dir in [APK_TMP_OUT, APK_TMP_SHELL, APK_TMP_IN]:
            for tmp_file in os.listdir(tmp_dir):
                tmp_abs_path = tmp_dir + '/' + tmp_file
                if os.path.isdir(tmp_abs_path):
                    shutil.rmtree(tmp_abs_path)
                else:
                    os.remove(tmp_abs_path)
        self.log.info('remove_all_tmp finish...')
