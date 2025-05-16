// Сообщение
using System;

public class Message
{
    public int Id { get; set; } // Идентификатор
    public string Sender { get; set; } // Отправитель
    public string Recipient { get; set; } // Получатель
    public string Content { get; set; } // Содержание
    public DateTime Timestamp { get; set; } // Временная метка
    public bool IsGroupMessage { get; set; } // true - отправлено группе, false - личное
    public bool IsOwnMessage { get; set; } // true - отправлено текущим пользователем, false - другим
    public bool IsDeleted { get; set; } // true - сообщение удалено из чата
}