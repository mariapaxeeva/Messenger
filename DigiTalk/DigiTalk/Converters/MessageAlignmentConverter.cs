// Конвертер для положения сообщения в области чата
// Если сообщение в чате принадлежит текущему пользователю, конвертер возвращает выравникание по правому краю, иначе - по левому

using System;
using System.Globalization;
using Avalonia.Data.Converters;
using Avalonia.Layout;

namespace DigiTalk.Converters
{
    public class MessageAlignmentConverter : IValueConverter
    {
        public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
        {
            if (value is bool isOwnMessage)
            {
                return isOwnMessage ? HorizontalAlignment.Right : HorizontalAlignment.Left;
            }
            return HorizontalAlignment.Left;
        }

        public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}