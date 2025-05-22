// Конвертер для управления доступностью элементов интерфейса
// Возвращает true, если объект не null, и false в противном случае

using Avalonia.Data.Converters;
using System;
using System.Globalization;

namespace DigiTalk.Converters
{
    public class ObjectIsNotNullConverter : IValueConverter
    {
        public static readonly ObjectIsNotNullConverter Instance = new();

        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return value != null;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotSupportedException();
        }
    }
}