function(rastertoolbox_apply_qt_defaults target_name)
    target_compile_definitions(${target_name} PRIVATE
        QT_NO_KEYWORDS
        QT_USE_QSTRINGBUILDER
    )
endfunction()
