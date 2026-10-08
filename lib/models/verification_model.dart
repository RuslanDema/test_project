enum CurrentScale { ma4_20, ma1_5 }

enum InspectionStatus { ok, error, pending }

/// Параметры и результаты поверки одного канала/датчика
class SensorVerificationConfig {
  String serialNumber;
  CurrentScale scale;
  double sMin; // мм
  double sMax; // мм
  double accuracyClass; // % (например 0.5)
  int pointsCount; // 3 или 5 точек

  // Результаты Операции 2: Проверка катушек
  double? rPrimary; // Wp (Ом)
  double? rSecondary; // Wc (Ом)
  InspectionStatus coilStatus;

  // Результаты Операции 1: Таблица точек
  List<MetrologyPointResult> points;

  SensorVerificationConfig({
    this.serialNumber = '',
    this.scale = CurrentScale.ma4_20,
    this.sMin = 0.0,
    this.sMax = 10.0,
    this.accuracyClass = 0.5,
    this.pointsCount = 5,
    this.rPrimary,
    this.rSecondary,
    this.coilStatus = InspectionStatus.pending,
    List<MetrologyPointResult>? points,
  }) : points = points ?? [];
}

/// Точка метрологического прогона
class MetrologyPointResult {
  final double nominalPercent; // 0%, 25%, 50%, 75%, 100%
  final double targetMm; // Эталонное перемещение, мм
  final double measuredMa; // Измеренный ток, мА
  final double calculatedMm; // Вычисленное перемещение, мм
  final double errorPercent; // Приведенная погрешность gamma, %
  final bool isPass; // Соответствие классу точности

  MetrologyPointResult({
    required this.nominalPercent,
    required this.targetMm,
    required this.measuredMa,
    required this.calculatedMm,
    required this.errorPercent,
    required this.isPass,
  });
}
