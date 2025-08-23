#define vSet(v, ...)                            \
    _Generic((v),                               \
        Vector2 *:  v2Set,                      \
        Vector3 *:  v3Set,                      \
        Vector4 *:  v4Set                       \
    )(v, __VA_ARGS__)

#define vMag(v)                                 \
    _Generic((v),                               \
        Vector2:    v2Mag,                      \
        Vector3:    v3Mag,                      \
        Vector4:    v4Mag                       \
    )(v)

#define vSum(v1, v2)                            \
    _Generic((v1),                              \
        Vector2:    v2Sum,                      \
        Vector3:    v3Sum,                      \
        Vector4:    v4Sum                       \
    )((v1), (v2))

#define vAdd(v1, v2)                            \
    _Generic((v1),                              \
        Vector2 *:  v2Add,                      \
        Vector3 *:  v3Add,                      \
        Vector4 *:  v4Add                       \
    )((v1), (v2))

#define vDiff(v1, v2)                           \
    _Generic((v1),                              \
        Vector2:    v2Diff,                     \
        Vector3:    v3Diff,                     \
        Vector4:    v4Diff                      \
    )((v1), (v2))

#define vSub(v1, v2)                            \
    _Generic((v1),                              \
        Vector2 *:  v2Sub,                      \
        Vector3 *:  v3Sub,                      \
        Vector4 *:  v4Sub                       \
    )((v1), (v2))

#define vTransform(m, v)                                            \
    _Generic((m),                                                   \
        Matrix3x3:                                                  \
            _Generic(v,                                             \
                Vector3:    v33Transform,                           \
                default:    (Vector3 (*)(Matrix3x3, Vector3)) NULL  \
            ),                                                      \
        Matrix3x2:                                                  \
            _Generic(v,                                             \
                Vector2:    v32Transform,                           \
                default:    (Vector3 (*)(Matrix3x2, Vector2)) NULL  \
            )                                                       \
    )(m, v)

#define mSetCoefficients(m, ...)                \
    _Generic((m),                               \
        Matrix2x2 *:  m2x2SetCoefficients,      \
        Matrix3x3 *:  m3x3SetCoefficients,      \
        Matrix4x4 *:  m4x4SetCoefficients       \
    )(m, __VA_ARGS__)

#define mPrint(fp, m, ...)                      \
    _Generic((m),                               \
        Matrix2x2:  m2x2Print,                  \
        Matrix2x3:  m2x3Print,                  \
        Matrix2x4:  m2x4Print,                  \
        Matrix3x2:  m3x2Print,                  \
        Matrix3x3:  m3x3Print,                  \
        Matrix3x4:  m3x4Print,                  \
        Matrix4x2:  m4x2Print,                  \
        Matrix4x3:  m4x3Print,                  \
        Matrix4x4:  m4x4Print                   \
    )(fp, m, __VA_ARGS__)
