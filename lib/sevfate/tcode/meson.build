tokenizer_src = custom_target('tcode_tokenizer',
    command: [re2c_exec, '-W', '-T', '--case-ranges', '--depfile', '@DEPFILE@', '-o', '@OUTPUT@', '@INPUT@'],
    input: 'Tokenizer.l', depfile: 'Tokenizer.l.d', output: 'Tokenizer.cpp'
)
parser_src = custom_target('tcode_parser',
    command: [
        lemon_exec, '-s',
        '-T@CURRENT_SOURCE_DIR@/LemonTemplate.cpp',
        '-H@CURRENT_SOURCE_DIR@/LemonTemplate.hpp',
        '-d@OUTDIR@', '@INPUT@'
    ],
    input: 'Parser.yy', output: ['Parser.hpp', 'Parser.cpp'],
    depend_files: ['LemonTemplate.cpp', 'LemonTemplate.hpp'], console: true
)

sevf_tcode_src += [tokenizer_src, parser_src]
sevf_tcode_src += files(
    'ParserDispatcher.cpp', 'ParserDispatcherConn.cpp',
    'ParserDispatcherRequest.cpp', 'ParserDispatcherResponse.cpp',
    'ParserDispatcherRegistry.cpp'
)
sevf_tcode_next_src += files('ParserDispatcherPendSync.cpp')
